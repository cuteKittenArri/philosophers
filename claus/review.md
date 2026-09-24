# Code review — `philo/` at commit `6f29fd2`

*Reviewed 2026-09-21. Nothing in the repository was modified. Findings marked
**verified** were reproduced by building and running a scratch copy of your code outside
the repository (with a minimal, test-only `sim()` that creates and joins the threads,
because yours is unfinished). Findings marked **by reading** come from the source alone.
Where `plan.md` already describes a fix, the finding points there instead of repeating it.
Line numbers refer to your files at `6f29fd2`.*

## Summary

| # | Severity | Where | Finding |
|---|---|---|---|
| R1 | Blocker | `routines.c:47` | Without a 5th argument, every philosopher stops after one meal |
| R2 | Blocker | `routines.c:5–21` | One philosopher hangs forever |
| R3 | Blocker | `routines.c:10,12,17,19` | Log text is `has taken a knife(scary)`, not `has taken a fork` |
| R4 | High | `routines.c:36–54` | Odd philosopher counts starve philosophers 1 and 5 |
| R5 | High | monitor design | Full philosophers leave, then get reported dead |
| R6 | High | `main.c:63`, `raidbots.c` | Doesn't build yet |
| R7 | Medium | `philo/philo` | Compiled binary committed; still no Makefile |
| R8 | Medium | 5 files | Norminette errors |
| R9 | Medium | `main.c:41–44` | A 5th argument of `0` or an overflow silently means "no limit" |
| R10 | Medium | `main.c:63` | `main` never cleans up |
| R11 | Low | `time_helprs.c:14–21` | `eepy` ignores `died`, so the program exits slowly |
| R12 | Low | `time_helprs.c:9–10` | `get_time` error path returns garbage |
| R13 | Low | several | Small things: includes, `%lu`, `philo_count`, a naming note |

Below the findings: a heads-up for writing the monitor, what is already solid, and how
all of this was tested.

---

## R1 — Without a 5th argument, every philosopher stops after one meal

**Blocker · verified**

```c
		if (philo->ate >= philo->env->hungry)
			break ;
```

With no 5th argument, `parsing` sets `hungry = -1`. After the first meal `ate` is 1, and
`1 >= -1` is true, so every philosopher leaves its loop.

**Measured:** `./philo 5 800 200 200` — each philosopher ate exactly once and the program
exited after 0.95 s. It must keep going until someone dies.

**Fix:** only apply the limit when there is one:

```c
		if (philo->env->hungry != -1 && philo->ate >= philo->env->hungry)
			break ;
```

With that one change, all five kept eating (30 meals in 3 s) until killed.

## R2 — One philosopher hangs forever

**Blocker · verified**

With `n_philo == 1`, `init_philos` correctly gives `l_mtx == r_mtx == &knifes[0]` — one
knife. But in `grab_em`, `l_mtx < r_mtx` is then false, so the `else` branch locks
`knifes[0]` twice. The second lock waits for a knife the thread already holds.

**Measured:** `./philo 1 800 200 200` prints `50 1 has taken a knife(scary)` and never
ends. A stack sample shows the thread waiting in `pthread_mutex_lock`, called from
`grab_em`. Even once a monitor prints the death, `pthread_join` would wait for this thread
forever, so the program could never exit. This exact test is on the evaluation sheet.

**Fix:** a separate path for one philosopher, checked before anything else in `routine`
(`plan.md` Step 4 describes it): take the one knife, print, wait out `times.die`, release
the knife, return. Releasing matters — destroying a mutex that is still locked is
undefined. `philo->l_mtx == philo->r_mtx` is a clean way to detect the case. See the
combined `routine` after R4.

With the fix: `0 1 has taken a fork`, then `801 1 died`, then a clean exit (3 of 3 runs).

## R3 — Wrong log text

**Blocker · by reading, visible in every run**

All four knife messages are `printer(philo, "has taken a knife(scary)")`. The subject
fixes the exact format — `timestamp_in_ms X has taken a fork` — and evaluators and testers
compare the text literally, so any other wording fails. Keeping `knifes` in *identifiers*
is fine; output is different. `plan.md` Step 4 notes that the log strings are the one
place where the subject's wording is mandatory.

**Fix:** `printer(philo, "has taken a fork");` in all four places.

## R4 — Odd philosopher counts starve philosophers 1 and 5

**High · verified**

With R1 fixed and a standard test-only death monitor added, your routine gives:

| Test | Result |
|---|---|
| `./philo 5 800 200 200` | nobody died in 3 × 8 s, but the worst gap between two meals was **800 ms — exactly `time_to_die`**, in all three runs. Meals over 8 s were uneven, e.g. 1→12, 2→20, 3→20, 4→21, 5→13 |
| `./philo 5 610 200 200` | **philosopher 5 died in 3 of 3 runs**, at 811, 1411 and 811 ms |
| `./philo 4 410 200 200` | nobody died; worst gap 403 ms — even counts are fine |

`5 800 200 200` is the first "nobody should die" test on the evaluation sheet. Zero margin
means any extra load on the evaluation machine can turn it into a death.

**Why:** with an odd number, at most (n−1)/2 philosophers can eat at once, so each one
must wait about 3 × `time_to_eat` between meals. Your routine has no think time: a
philosopher that wakes up competes for knifes again immediately, and mutexes aren't fair.
Philosophers 1 and 5 both need `knifes[0]` first, so they are the ones that keep losing.

**Fix:** for odd counts, think long enough that each cycle is 3 × `time_to_eat`:

```c
		printer(philo, "is thinking");
		if (philo->env->n_philo % 2 == 1
			&& philo->times.nom * 2 > philo->times.sleep)
			eepy(philo->times.nom * 2 - philo->times.sleep);
```

The `>` check keeps the unsigned subtraction from wrapping around. Measured with the fix:
`5 800 200 200` worst gap 600 ms, with 14 meals each (±1); `5 610 200 200` nobody died,
3 of 3.

### `routine` with R1, R2 and R4 applied

Verified: compiles with `-Wall -Wextra -Werror`, passes norminette, and passes every test
in the table under "How this was tested". `solo_philo` is the name `plan.md` Step 4 uses;
any name works.

```c
void	*solo_philo(t_philo *philo)
{
	pthread_mutex_lock(philo->l_mtx);
	printer(philo, "has taken a fork");
	eepy(philo->times.die);
	pthread_mutex_unlock(philo->l_mtx);
	return (NULL);
}

void	*routine(void *me)
{
	t_philo	*philo;

	philo = (t_philo *)me;
	if (philo->l_mtx == philo->r_mtx)
		return (solo_philo(philo));
	if (philo->id % 2 == 1)
		eepy(50);
	while (!died(philo->env))
	{
		mahlzeit(philo);
		if (philo->env->hungry != -1 && philo->ate >= philo->env->hungry)
			break ;
		printer(philo, "is sleeping");
		eepy(philo->times.sleep);
		printer(philo, "is thinking");
		if (philo->env->n_philo % 2 == 1
			&& philo->times.nom * 2 > philo->times.sleep)
			eepy(philo->times.nom * 2 - philo->times.sleep);
	}
	return (NULL);
}
```

## R5 — Full philosophers leave, then get reported dead

**High · verified with a test monitor · matters when you write Step 5**

Once R1 is fixed, a philosopher that has eaten `hungry` times `break`s out of its loop and
never updates `last_nom` again. If the monitor keeps checking it, it will report that
philosopher dead as soon as `time_to_die` has passed — even though it is simply done.

**Measured:** `./philo 5 800 200 200 7` (a mandatory test), with a monitor that checks
every philosopher: 1 of 3 runs ended with `3201 2 died`, although philosopher 2 had
already eaten 7 of 7 and left. It's intermittent, because it depends on how far apart the
philosophers finish, and R4's uneven meal counts widen that gap. With the monitor skipping
full philosophers: 3 of 3 clean.

**Fix, for Step 5:** in the monitor's death check, skip any philosopher with
`hungry != -1 && ate >= hungry` (read under `nom_mtx`, like `last_nom`), and stop the
monitor once every philosopher is full.

## R6 — Doesn't build yet

**High · verified**

Expected while `sim` is unfinished, but so the list is complete:

- `main.c:63` calls `sim`, which isn't declared in `func.h`:
  `call to undeclared function 'sim'`.
- `raidbots.c:8` stops at `threads =`, giving `expected expression`, and `env` is unused,
  which `-Werror` rejects.
- `routine` needs a prototype in `func.h` too, because `sim` will pass it to
  `pthread_create` from another file.

Your latest commit removed `thread_id` from `t_philo`, so the thread handles need a new
home. With your decision to use fixed arrays instead of `malloc`, a local
`t_id threads[P_MAX];` inside `sim` is the natural place. (`plan.md`'s glossary still
lists `thread_id` in `t_philo`; that row is now out of date.)

## R7 — Compiled binary committed; still no Makefile

**Medium · verified**

`philo/philo` is a Linux x86-64 executable, built on your other machine. It cannot match
the current sources, since they don't compile. The subject lists only `Makefile, *.h, *.c`
in `philo/`, and anyone who runs `./philo` without building first gets a stale program.

**Fix:** `git rm --cached philo/philo`, plus a `.gitignore` containing `philo/philo` and
`*.o`. The Makefile itself is mandatory and still missing — `plan.md` Step 7 has one; add
your new files to its `SRCS`.

## R8 — Norminette errors

**Medium · verified**

| File | Line | Error | Fix |
|---|---|---|---|
| `time_helprs.c` | 1 | `INVALID_HEADER`, `EMPTY_LINE_FILE_START` | Add the 42 header, and drop the blank first line |
| `routines.c` | 1 | `INVALID_HEADER` | Add the 42 header |
| `raidbots.c` | — | Can't be parsed yet (unfinished) | Will also need the 42 header |
| `routines.c` | 38 | `SPACE_REPLACE_TAB` | `t_philo *philo;` needs a tab between type and name |
| `routines.c` | 41 | `EMPTY_LINE_FUNCTION`, `SPACE_EMPTY_LINE` | Delete the line that holds only a tab |
| `pthread_helper.c` | 41 | `NL_AFTER_VAR_DECL` | Empty line after `bool oof;` in `died()` |
| `main.c` | 36 | `LINE_TOO_LONG` | Split the condition as in `plan.md` B.5 |

Already clean: `philomilo.h`, `func.h`, `helprs.c`, `ini_mini.c`.

## R9 — A 5th argument of `0` or an overflow silently means "no limit"

**Medium · by reading**

`main.c:41–44` still turns any non-positive result into `-1`, so
`./philo 5 800 200 200 0` and `./philo 5 800 200 200 99999999999` both run with no meal
limit instead of stopping or reporting an error. The fix is `plan.md` B.3.

## R10 — `main` never cleans up

**Medium · by reading**

After `sim(&env);`, `main` ends without destroying any mutex and without a `return`. Your
`destroyer(&env, NULL, env.n_philo)` already does exactly this cleanup and returns 0, so
end `main` with `return (destroyer(&env, NULL, env.n_philo));`, as in `plan.md` B.6. It
must run after every thread has been joined: destroying a mutex that is still locked is
undefined, which is one more reason R2's one-philosopher path has to release its knife.

## R11 — `eepy` ignores `died`, so the program exits slowly

**Low · by reading**

`eepy` always sleeps its full duration. After a death, `printer` already suppresses
output, so nothing wrong gets printed — but each philosopher first finishes its current
eat, sleep and (with R4) think, so the program can take up to about
`nom + sleep + think` ms to exit after the `died` line. Checking `died()` inside `eepy`'s
loop makes the exit prompt; that needs `env` (or the philosopher) as a parameter, which is
why `plan.md` Step 3's version takes one.

## R12 — `get_time` error path returns garbage

**Low · by reading**

If `gettimeofday` fails, `ende` prints a message but `get_time` carries on and returns an
uninitialised value. `gettimeofday` can't realistically fail here (valid pointer, `NULL`
timezone), so the simplest fix is to drop the check; otherwise return `0` after `ende`.

## R13 — Small things

**Low · by reading**

- **Unused includes:** `<sched.h>` in `time_helprs.c` and `<time.h>` in `routines.c`.
  Norminette doesn't catch these, but Norm III.5 forbids unused headers and an evaluator
  can flag them.
- **Redundant includes:** `"philomilo.h"` in `main.c`, `routines.c` and `raidbots.c`, and
  `<pthread.h>` in `ini_mini.c`, are already pulled in by `func.h`. Harmless.
- **`%lu` for `size_t`:** `printf("%lu …", now)` works on your machines, where `size_t` is
  `unsigned long`, but `%zu` is the format made for `size_t`.
- **`philo_count`** in `t_env` is still unused; `n_philo` is the live one.
- **`died`** is both a field (`env->died`) and a function (`died(env)`). That is legal —
  struct members live in their own namespace — but be ready to explain it if an evaluator
  asks.

---

## Heads-up for Step 5: printing the death

Your `printer()` checks `died()` while holding `print_mtx`, so once `death()` has run,
`printer` prints nothing — a `died` line included. That rules out calling `death(env)`
and then `printer(philo, "died")`. The reverse order is no better: after `printer`
unlocks and before `death()` runs, another philosopher's line can slip in *after* `died`.
Do both inside one `print_mtx` section:

```c
	pthread_mutex_lock(&env->print_mtx);
	printf("%zu %d died\n", get_time() - env->times.start, env->philos[i].id);
	death(env);
	pthread_mutex_unlock(&env->print_mtx);
```

The lock order is `print_mtx` then `died_mtx` — the same as in `printer` — so the two can't
deadlock each other. **Measured** with a test monitor using exactly this: in 20 of 20 runs
of `./philo 4 310 200 100`, `died` was the last line, printed at 311–312 ms (actual death
at 310 ms, allowed up to 320 ms).

## What is already solid

- `init_env`, `destroyer`, `ende` (plan.md B.1, B.2, B.4), `init_philos`, and the
  `arg_checker` loop and philosopher-count fixes are all in. `ini_mini.c`, `helprs.c`,
  `philomilo.h` and `func.h` pass norminette.
- `grab_em`'s ordering is deadlock-free — shown in `plan.md` C.2.
- `mahlzeit` updates `last_nom` and `ate` under `nom_mtx`, and stays clean under
  ThreadSanitizer even with a monitor reading them — shown in `plan.md` C.1.
- `printer` takes the timestamp *inside* `print_mtx`, so lines always come out in time
  order, and its lock order (`print_mtx`, then `died_mtx`) matches the plan's invariant.
- With a 5th argument, the routine already works: `./philo 5 800 200 200 3` ate all 15
  meals and exited cleanly. `4 410 200 200` and `4 310 200 100` behave correctly too.

## How this was tested

A copy of `philo/` in a temporary directory outside the repository, compiled with
`cc -Wall -Wextra -Werror`. Because `sim()` is unfinished, the copy got a test-only one
that creates and joins the threads. Each experiment changed exactly one thing through a
compiler `-D` switch, so the default build was your code as written. The tools:
ThreadSanitizer (`-fsanitize=thread`) for data races, macOS `sample` for stack traces of
hung processes, and a small test-only monitor for the death and meal-limit runs. Nothing
was built, changed or added inside `philo/`, and `git status` was checked after every step.

Final check, with R1–R4 applied to the copy and a monitor that follows R5 and the Step 5
heads-up — three runs each:

| Test | Result |
|---|---|
| `./philo 1 800 200 200` | `0 1 has taken a fork`, then `801 1 died`, then exits |
| `./philo 5 800 200 200` | nobody died |
| `./philo 5 800 200 200 7` | exits by itself, exactly 7 meals each, nobody died |
| `./philo 4 410 200 200` | nobody died |
| `./philo 4 310 200 100` | died at 311–312 ms, as it should |
| `./philo 5 610 200 200` | nobody died |
| `./philo 200 800 200 200` | nobody died |
| ThreadSanitizer, `5 800 200 200 7` | 0 data races |
