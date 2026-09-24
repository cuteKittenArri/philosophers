# Review 2 — `philo/` at `6be7222`

*2026-09-24. Nothing in the repo was changed. Tested on a scratch copy on macOS; there
is no Linux or Valgrind here, so `<linux/limits.h>` was swapped for an empty stand-in to
make it compile. All results are from real runs, 3 per case unless noted. Line numbers
refer to your files at `6be7222`.*

## Results as the code stands

| Test | Should | Does |
|---|---|---|
| `1 800 200 200` | die at ~800, exit | ✓ `801 1 died`, exits |
| `4 310 200 100` | die at ~310, exit | dies at 311 ✓, then **hangs** 3/3 |
| `5 800 200 200` | nobody dies | **dies** 3/3, then hangs |
| `5 800 200 200 7` | 7 meals each, exit | **dies** 1/3 (and hangs), fine 2/3 |
| `4 410 200 200` (also with `5`) | nobody dies | **dies** every run, at 611 ms |
| `2 410 200 200` | nobody dies | **dies** 2/2, at 611 ms |
| `5 610 200 200`, `3 610 200 200` | nobody dies | **die** every run, then hang |
| `200 800 200 200` | nobody dies | **dies** 2/2 at ~1003 ms, then hangs |
| `4 800 100 300` | nobody dies | **dies** 2/2 at 901 ms, then hangs |
| `5 250 200 100 1` | one dies (can't eat before 250) | **no `died` line**, 3/3 |
| 7 invalid-argument cases | error, exit 1 | ✓ all |
| ThreadSanitizer | no data races | ✓ none |

What already works: every `died` line came within 1 ms of the deadline (the limit is
10 ms), nothing was ever printed after it, and the one-philosopher case is correct apart
from its log text.

## Problems

| # | Severity | Where | Problem |
|---|---|---|---|
| 1 | Blocker | `routines.c:62–63` | Think time depends on the philosopher's id, not on the count |
| 2 | Blocker | `routines.c:63` | `nom * 2 - sleep` wraps around when sleep > 2 × eat |
| 3 | Blocker | `raidbots.c:30–35` | Monitor returns with `nom_mtx` still locked → hang |
| 4 | Blocker | `raidbots.c:26–38` | `full` is never reset → monitor stops too early |
| 5 | Blocker | `routines.c:11,13,18,20,40` | Log says `has taken a knife(scary)` |
| 6 | Blocker | `philo/` | No Makefile |
| 7 | High | repo root | No `README.md` |
| 8 | Medium | `raidbots.c:28–41` | Monitor never sleeps: one CPU core at 100 % |
| 9 | Medium | `raidbots.c:3` | `<linux/limits.h>` is unused and breaks the macOS build |
| 10 | Medium | 5 files | Norminette errors |
| 11 | Low | `raidbots.c:52`, `pthread_helper.c:59` | `start` is set twice; `died` uses the older value |
| 12 | Low | several | Small things |

### 1 — Think time depends on the id, not the count

```c
		if (philo->id % 2 == 1)
			eepy(philo->times.nom * 2 - philo->times.sleep);
```

The extra think time is only meant for an **odd number of philosophers** (`claus/review.md`
R4). This line checks whether the philosopher's **number** is odd instead, so it slows down
half the philosophers in every simulation:

- **Even counts:** eat + sleep already fits, so the extra 200 ms kills.
  `4 410 200 200`: philo 3 eats at 200, sleeps until 600, then thinks until 800. Its
  deadline was 200 + 410 = 610, so it prints `611 3 died`. The same happens with
  `2 410 200 200`.
- **Odd counts:** only the odd ids wait, so the even ids get back to the table 200 ms
  earlier and win the shared knifes again and again. `5 800 200 200`: philo 1 last ate at
  200 and died at 1001.

**Fix:** check the count, and keep the guard from #2:

```c
		if (philo->env->n_philo % 2 == 1
			&& philo->times.nom * 2 > philo->times.sleep)
			eepy(philo->times.nom * 2 - philo->times.sleep);
```

**Confirmed:** with only this change, all seven "nobody dies" cases in the table ran
without a death, 3 of 3.

### 2 — Wrap-around when sleep > 2 × eat

`times.nom` and `times.sleep` are `size_t`, which can't go negative. `4 800 100 300`:
100 × 2 − 300 = −100, which wraps to 18 446 744 073 709 551 516 ms, so `eepy` never
returns. Philosophers 1 and 3 never eat again (`901 1 died`), and `sim` waits for them
forever. A stack sample of the hung process shows exactly that: 2 threads in
`routine > eepy`, and main waiting in `sim`.

**Fix:** the `nom * 2 > sleep` guard in #1.

### 3 — Monitor returns with `nom_mtx` still locked

```c
		pthread_mutex_lock(&env->nom_mtx);
		...
			if (!alive)
				return (print_death(&env->philos[i]));
```

The `return` skips the unlock on line 39, so `nom_mtx` stays locked for good. The next
philosopher to start a meal blocks in `mahlzeit` on `nom_mtx` while holding both its
knifes. Its neighbours then block in `grab_em`, and `sim` waits in `pthread_join` forever.

**Measured:** `4 310 200 100` printed `311 2 died` and hung, 3 of 3. The stack sample
shows 2 threads in `routine > mahlzeit` blocked on a mutex, and main waiting in `sim`.
Every run with a death hung the same way, unless nobody happened to start a meal after it
(`4 410 200 200`). When the program does exit, `destroyer` destroys the still-locked
`nom_mtx`, which POSIX leaves undefined.

**Fix:** unlock before printing (see "Combined fix" below). **Confirmed:** with only this
change, `4 310 200 100` exited after the death, 3 of 3.

Also, `return (print_death(...))` returns a `void` expression from a `void` function.
That isn't valid ISO C (`clang -Wpedantic` warns about it). Call the function, then
`return ;`.

### 4 — `full` is never reset

`full` should count the philosophers who are full, but it only ever grows, and it grows
**every time** the monitor passes a full philosopher. The monitor laps the table without
pausing, so a single full philosopher gets passed `n_philo` times within microseconds.
`full` then reaches `n_philo` and the monitor quits while the others are still eating.

**Measured:** `5 250 200 100 1`. Philosophers 2 and 4 eat at 0 ms, which makes them full
immediately, and the monitor stops. Philosopher 1 (or 5) can't eat before 400 ms, so it
should die at 250, but no `died` line appeared, 3 of 3.

**Fix:** set `full = 0` whenever a philosopher is *not* full. Then `full == n_philo`
means `n_philo` full philosophers in a row, which is all of them. **Confirmed:** with only
this change, `251 1 died`, 3 of 3.

### Combined fix for 3, 4 and 8

Verified: it compiles, passes norminette at exactly 25 lines (`alive` had to go to fit),
and gets no ThreadSanitizer reports.

```c
static void	death_checker(t_env *env)
{
	int	i;
	int	full;

	full = 0;
	i = 0;
	while (full < env->n_philo)
	{
		pthread_mutex_lock(&env->nom_mtx);
		if (env->hungry == -1 || env->philos[i].ate < env->hungry)
		{
			full = 0;
			if (!u_good(&env->philos[i]))
			{
				pthread_mutex_unlock(&env->nom_mtx);
				print_death(&env->philos[i]);
				return ;
			}
		}
		else
			full++;
		pthread_mutex_unlock(&env->nom_mtx);
		i = (i + 1) % env->n_philo;
		if (i == 0)
			usleep(500);
	}
}
```

### 5 — Log text

There are five `printer(philo, "has taken a knife(scary)")` calls: four in `grab_em` and
one in `lonely`. The subject fixes the wording as `X has taken a fork`, and evaluators and
testers compare it literally. `knifes` as an identifier is fine; the output text isn't.

### 6 — No Makefile

The subject requires a `Makefile` in `philo/` with the rules `NAME`, `all`, `clean`,
`fclean` and `re`, and it must not relink. Evaluation starts with `make`.
`claus/plan.md` Step 7 has one; change its source list to
`SRCS = main.c helprs.c ini_mini.c pthread_helper.c time_helprs.c routines.c raidbots.c`.

### 7 — No `README.md`

Chapter VII requires `README.md` at the repo root. Its first line must be in italics:
`*This project has been created as part of the 42 curriculum by stmuller.*` After that
come Description, Instructions and Resources sections, and Resources must say how AI was
used. See `claus/plan.md` Step 8.

### 8 — Monitor never sleeps

`death_checker` laps the table with no pause. **Measured:** `1 800 200 200` used 0.80 s
of CPU in 0.80 s, one full core; `5 800 200 200 7` used 3.4 s of CPU in 4.4 s. It also
takes `nom_mtx` thousands of times per millisecond, competing with the philosophers for
it. Adding a `usleep(500)` per lap (done in the combined fix) brings that down to 0.00 s
and 0.05 s, and deaths are still reported within 1 ms.

### 9 — `<linux/limits.h>`

Nothing in `raidbots.c` uses it, and it only exists on Linux, so the project doesn't
compile on macOS (`fatal error: 'linux/limits.h' file not found`). Delete it. The other
includes in that file — `<sched.h>`, `<time.h>`, `<stdbool.h>` and `<pthread.h>` — are
either unused or already pulled in by `func.h`.

### 10 — Norminette

| File | Line | Error |
|---|---|---|
| `routines.c`, `raidbots.c`, `time_helprs.c` | 1 | `INVALID_HEADER`: no 42 header |
| `time_helprs.c` | 1 | `EMPTY_LINE_FILE_START` |
| `raidbots.c` | 44–45 | `CONSECUTIVE_NEWLINES`: three empty lines before `sim` |
| `routines.c` | 47 | `SPACE_REPLACE_TAB`: `t_philo *philo;` needs a tab |
| `pthread_helper.c` | 43 | `NL_AFTER_VAR_DECL`: empty line needed after `bool oof;` |
| `main.c` | 37 | `LINE_TOO_LONG`: split it as in `claus/plan.md` B.5 |

`philomilo.h`, `func.h`, `helprs.c` and `ini_mini.c` are clean. Also worth knowing:
`return (lonely(philo), NULL);` and `return (ende(...), 0);` use the comma operator.
Norminette accepts it, but it is two instructions on one line, which an evaluator can
count as a Norm violation.

### 11 — `start` is set twice

`init_philos` sets `env->times.start` and copies it into every `philo->times`. Then `sim`
(`raidbots.c:52`) sets `env->times.start` again. `printer` uses the new value, but
`print_death` uses the philosopher's old copy (`philo->times.start`). So if a millisecond
ticks over between the two, `died` is stamped 1 ms later than every other line. Delete
line 52.

### 12 — Small things

- `pthread_create`'s return value is ignored (`raidbots.c:55`). If it fails,
  `pthread_join` gets an uninitialised handle.
- `eepy` ignores `died`, so after a death each philosopher still finishes its current
  eat, sleep or think before the program can exit (`claus/review.md` R11).
- `t_env.philo_count` is unused.
- Redundant includes: `"philomilo.h"` and `<pthread.h>` in several files, `<time.h>` in
  `main.c` and `routines.c`, and `<sched.h>` in `time_helprs.c`.

## After fixes 1–5 and 8

These are the same tests, run on the scratch copy with the `routine` change from #1, the
combined `death_checker` and `has taken a fork`:

| Test | Result (3 runs each) |
|---|---|
| `1 800 200 200` | `801 1 died`, exits |
| `4 310 200 100` | `311 2 died`, exits |
| `5 250 200 100 1` | `251 1 died`, exits |
| `5 800 200 200 7`, `4 410 200 200 5` | everyone full, nobody died, exits |
| `5 800 200 200`, `4 410 200 200`, `2 410 200 200`, `5 610 200 200`, `3 610 200 200`, `200 800 200 200`, `4 800 100 300` | nobody died |
| ThreadSanitizer | 0 reports |
