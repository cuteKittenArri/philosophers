# Philosophers — Plan to Finish the Mandatory Part

## Context

The `philo/` directory holds a parsing/init skeleton — roughly 30% of the project. It
**does not currently compile**, and three files fail norminette. Everything that makes
this a threading project (threads, the routine, death detection, synchronised printing)
is still unwritten, and there is no Makefile and no README.

This plan takes it to a submittable mandatory part. Bonus (`philo_bonus/`, processes +
semaphores) is explicitly out of scope, and per Chapter VIII it would not be graded
anyway unless the mandatory part is perfect.

Requirements were extracted from `subject.pdf` (Philosophers v13.0) and `norm.pdf`
(Norm v4.1), both at the repo root.

### Decisions already made

| Question | Decision |
|---|---|
| Struct layout | `t_philo` gets a back-pointer to `t_env`; `t_muthexe` is deleted |
| Naming | **Keep every existing name.** Nothing is renamed. New names must come from the glossary below |
| Allocation | Keep fixed `P_MAX` arrays; **no malloc**; parser must reject `n > 200` |
| `exit()` | Remove it — error paths return a status code, `main` ends with `return (1)` |

---

## Naming glossary — single source of truth

*Updated 2026-09-21 to match your code as of commit `f0a359f`. Steps 1–6 below were
written before that commit and still use some old names: `hungry` as the per-philosopher
counter, `must_nom`, `stop` / `stop_mtx` (and `is_stopped` / `set_stopped`),
`philo_count`, `thread`, and `long` where your times are `size_t`. Where they differ,
this table wins.*

Every identifier in this project comes from this vocabulary. **Do not import alternative
words** (no `meal`, `fork`, `fed`, `full`) from the subject PDF or from conventional 42
style, even when the subject's own argument names use them.

| Concept | Name | Lives in |
|---|---|---|
| a fork | `knifes[]` | `t_env` |
| this philosopher's two forks | `l_mtx`, `r_mtx` (pointers into `knifes[]`, see A.3) | `t_philo` |
| the act of eating | `nom()` | `routine.c` |
| how long eating takes | `times.nom` | `t_env.times`, copied into `t_philo.times` |
| when this philosopher last ate | `last_nom` | `t_philo` |
| how many times they have eaten | `ate` | `t_philo` |
| how many times they must eat (5th arg) | `hungry` | `t_env` |
| mutex guarding `last_nom` + `ate` | `nom_mtx` | `t_env` |
| "have they all eaten enough?" | `check_all_nommed()` | `monitor.c` |
| someone died, so everyone stops | `died` | `t_env` |
| mutex guarding `died` | `died_mtx` | `t_env` |
| mutex serialising log lines | `print_mtx` | `t_env` |
| number of philosophers | `n_philo` | `t_env` |
| this philosopher's thread | `thread_id` | `t_philo` |

`t_env` also still has `philo_count`, which nothing reads. `n_philo` is the live one, so
`philo_count` can go.

The subject calls the optional 5th argument
`number_of_times_each_philosopher_must_eat` — that is the *subject's* word, and it stops
at the parser. Inside the code it is `env->hungry`, compared against each philosopher's
`ate`.

Note that keeping `nom` also sidesteps a Norm problem: the Norm reserves the `t_` prefix
for typedef names, so a `t_times` member called `t_eat` would read as a type. `nom` has
no such clash.

---

## Verified baseline

Compiler (`cc -Wall -Wextra -Werror`):
- `main.c:31` — `init_philos` undeclared; `TODO` undeclared
- `main.c:22` — unused parameter `argc`
- `pthread_helper.c:15` — unused parameter `e_msg`
- `helprs.c`, `ini_mini.c` compile clean

Norminette 3.3.59:
- `philomilo.h:26,35,51` — `TAB_REPLACE_SPACE` (tab after `typedef struct`)
- `main.c:18`, `ini_mini.c:20` — `MIXED_SPACE_TAB`, `CONSECUTIVE_SPC`, `TOO_FEW_TAB`
- `helprs.c`, `func.h`, `pthread_helper.c` — OK

Logic bugs found by reading:
- `arg_checker(argv + 1)` runs **before** `argc` is validated → out-of-bounds read on `./philo`
- `destroyer()` ignores `e_msg` and never halts → a failed mutex init falls through silently
- `init_env` uses `||`, so if `nom_mtx` fails, `print_mtx` is never initialised — yet
  `destroyer` destroys both → UB destroying an uninitialised mutex
- `philotoi` returns `int` from a `long` → an 11-digit argument silently overflows
- Nothing rejects `n > P_MAX` → `./philo 300 ...` overflows both stack arrays
- `ende` writes to fd 1 (stdout); errors belong on fd 2

---

## Step 1 — Rewrite the headers

### `philo/philomilo.h`

Fix the three norminette errors: `typedef struct\ts_times` → `typedef struct s_times`
(single **space** after `struct`, on lines 26, 35, 51).

`t_muthexe` is **deleted**. Its `l_mtx`/`r_mtx` keep their names but move up onto
`t_philo`; `print_mtx`/`nom_mtx` already live in `t_env` and are now reached through the
back-pointer.

`t_times` **survives but moves**. Today each philosopher carries its own copy of
`start`/`die`/`nom`/`sleep`, but those four come from `argv`, are identical for every
philosopher, and never change after init — so one instance belongs in `t_env`. Only
`last_nom` was genuinely per-philosopher, so it moves up onto `t_philo`, keeping its name.

```c
# include <pthread.h>
# include <stdbool.h>
# include <stddef.h>

# define P_MAX 200

typedef pthread_t       t_id;
typedef pthread_mutex_t t_mutex;
typedef unsigned char   t_philo_id;

typedef struct s_env    t_env;          /* forward decl for the back-pointer */

typedef struct s_times
{
    long    start;
    long    die;
    long    nom;
    long    sleep;
}   t_times;

typedef struct s_philo
{
    t_philo_id  id;
    t_id        thread;
    t_mutex     *l_mtx;                 /* was muthexe.l_mtx */
    t_mutex     *r_mtx;                 /* was muthexe.r_mtx */
    t_env       *env;                   /* new: reaches stop flag + times */
    long        last_nom;               /* was times.last_nom */
    int         hungry;
}   t_philo;

struct s_env
{
    t_mutex     print_mtx;
    t_mutex     nom_mtx;
    t_mutex     stop_mtx;               /* new */
    t_mutex     *knifes;
    t_philo     *philos;
    t_times     times;                  /* now the single shared copy */
    int         philo_count;
    int         must_nom;               /* new: -1 when argc == 5 */
    bool        stop;                   /* new */
};
```

Every name here is either original or drawn from the glossary. The only genuinely new
concepts are the stop flag and its mutex, which have no existing vocabulary to clash with.

**Watch for:** norminette can be fussy about the bodyless `typedef struct s_env t_env;`.
If it complains, drop that line and write `struct s_env  *env;` inside `t_philo` instead —
equally valid, since `struct s_env` is fully defined later in the same header.

Also note `t_philo_id` is `unsigned char`, which holds 1–200 fine. It only breaks if
`P_MAX` ever goes above 255.

### `philo/func.h`

Prototypes for every function below. Fix the includes — `<stdbool.h>` is already pulled
in by `philomilo.h`, and `<stdlib.h>` becomes unused once `exit` is gone (Norm III.5
starred rule: "Inclusion of unused headers is forbidden"). Needed: `<unistd.h>` for
`usleep`/`write`, `<stdio.h>` for `printf`, `<sys/time.h>` for `gettimeofday`.

---

## Step 2 — Fix parsing and error handling

### `philo/helprs.c` (5 functions — at the Norm limit)

- `is_digit` — unchanged
- `is_legal` — drop the 12-char cap; just "non-empty and all digits". Range enforcement
  moves to the parser, which keeps the two concerns separate.
- `philotoi` — **return `long`, not `int`**, and detect overflow inline. No `limits.h`
  needed:

```c
long    philotoi(char *str)
{
    long    ret;

    ret = 0;
    while (*str)
    {
        ret = (ret * 10) + (*str - '0');
        if (ret > 2147483647)
            return (-1);
        str++;
    }
    return (ret);
}
```

- `ft_strlen` — unchanged
- `ende` — **no `exit`**. Write to fd 2, return 1 so callers can `return (ende(...))`:

```c
int     ende(char *e_msg)
{
    if (e_msg)
        write(2, e_msg, ft_strlen(e_msg));
    write(2, "\n", 1);
    return (1);
}
```

### `philo/args.c` (new)

- `check_args(int argc, char **argv)` — **validate `argc` is 5 or 6 first**, then run
  `is_legal` on `argv[1..argc-1]`. This ordering is the fix for the out-of-bounds read.
- `parse_args(t_env *env, int argc, char **argv)` — fill `philo_count`, `times`,
  `must_nom`, enforcing:
  - `philo_count` in `[1, P_MAX]` — **the array-overflow fix**
  - `times.die`, `times.nom`, `times.sleep` each `>= 1`
  - `must_nom >= 0` when `argc == 6`, else `-1`
  - reject any `philotoi` result of `-1` (overflow)

Judgement calls worth knowing: `must_nom == 0` means everyone is trivially done, so exit
cleanly at once without starting threads. Requiring the three times to be `>= 1` rejects
`./philo 5 0 0 0`; the alternative is to accept 0 and let everyone die instantly. Either
is defensible — flagged so you can answer it at defence.

---

## Step 3 — Time, printing, and the stop flag

### `philo/utils_time.c` (new, 5 functions — at the limit)

```c
long  get_time_ms(void);                        /* gettimeofday -> ms */
void  precise_sleep(t_env *env, long ms);       /* short usleep loop */
void  print_state(t_philo *philo, char *msg);
bool  is_stopped(t_env *env);
void  set_stopped(t_env *env);
```

`precise_sleep` must **not** be a single `usleep(ms * 1000)` — that overshoots and blows
the subject's 10 ms death-report window. Loop on a deadline, bailing early if stopped:

```c
deadline = get_time_ms() + ms;
while (get_time_ms() < deadline)
{
    if (is_stopped(env))
        return ;
    usleep(200);
}
```

### ⚠ Lock ordering — this is where this project deadlocks

Three mutexes are in play. The obvious implementation of `print_state` (take `print_mtx`,
then check the stop flag) races head-on with the monitor (which holds the nom data, then
wants to print) and produces a classic ABBA deadlock.

**The invariant to hold everywhere:**

> `nom_mtx` is always acquired and released **alone**, never while holding another mutex.
> When both `print_mtx` and `stop_mtx` are needed, **`print_mtx` is always taken first.**

That makes the philosopher path and the monitor's death path use the *same* order, so
they cannot deadlock:

```c
void    print_state(t_philo *philo, char *msg)      /* print_mtx -> stop_mtx */
{
    long    stamp;

    pthread_mutex_lock(&philo->env->print_mtx);
    stamp = get_time_ms() - philo->env->times.start;
    if (!is_stopped(philo->env))
        printf("%ld %d %s\n", stamp, philo->id, msg);
    pthread_mutex_unlock(&philo->env->print_mtx);
}
```

The monitor sets `stop` **while still holding `print_mtx`**, which is what guarantees the
`died` line is the last thing printed — no philosopher can slip a line in behind it.

---

## Step 4 — The philosopher routine

### `philo/routine.c` (new, 4 functions)

- `routine(void *arg)` — thread entry
- `solo_philo(t_philo *philo)` — the `n == 1` case
- `take_knifes(t_philo *philo)`
- `nom(t_philo *philo)`

**Deadlock avoidance.** Because `knifes` is one contiguous array, comparing the two
pointers gives a total order over all forks, which provably breaks the circular wait —
and it needs only `if`/`else`, no ternary:

```c
if (philo->l_mtx < philo->r_mtx)
{
    pthread_mutex_lock(philo->l_mtx);
    print_state(philo, "has taken a fork");
    pthread_mutex_lock(philo->r_mtx);
    print_state(philo, "has taken a fork");
}
else
{ /* mirror image */ }
```

Add a stagger so even-numbered philosophers don't all grab at once:
`if (philo->id % 2 == 0) precise_sleep(env, env->times.nom / 2);` before the main loop.

**`nom()` updates `last_nom` and `hungry` under `nom_mtx`, alone**, then releases before
printing or sleeping:

```c
take_knifes(philo);
pthread_mutex_lock(&philo->env->nom_mtx);
philo->last_nom = get_time_ms();
philo->hungry++;
pthread_mutex_unlock(&philo->env->nom_mtx);
print_state(philo, "is eating");
precise_sleep(philo->env, philo->env->times.nom);
pthread_mutex_unlock(philo->l_mtx);
pthread_mutex_unlock(philo->r_mtx);
```

Main loop shape:

```c
while (!is_stopped(philo->env))
{
    nom(philo);
    print_state(philo, "is sleeping");
    precise_sleep(philo->env, philo->env->times.sleep);
    print_state(philo, "is thinking");
}
```

**`solo_philo` is mandatory, not an optimisation.** With one philosopher there is one
fork; the generic path would lock the same mutex twice and hang. Take the one fork, print
`has taken a fork`, wait out `times.die`, let the monitor report the death.

Note the log strings themselves (`is eating`, `has taken a fork`) are fixed by the subject
and are the one place the subject's vocabulary is mandatory — they are output, not
identifiers.

---

## Step 5 — The monitor

### `philo/monitor.c` (new, 3 functions)

- `monitor(t_env *env)` — poll loop, `usleep(500)` between sweeps (well inside the 10 ms
  requirement)
- `check_death(t_env *env, t_philo *philo)` — read `last_nom` under `nom_mtx`, release,
  then `if (elapsed < env->times.die) return (0);` and otherwise do the print→stop
  sequence from Step 3
- `check_all_nommed(t_env *env)` — no-op when `must_nom < 0`; otherwise count philosophers
  with `hungry >= env->must_nom` under `nom_mtx` and stop when all qualify

**Run the monitor on the main thread**, after the create loop — no extra thread to spawn
or join.

---

## Step 6 — Thread lifecycle and cleanup

### `philo/ini_mini.c` (keep name, 3 functions)

- `init_env` — set counts/times/flags, `times.start = get_time_ms()`, and initialise the
  three mutexes. **Fix the `||` short-circuit bug**: initialise one at a time and unwind
  only what actually succeeded.
- `init_knifes` — unchanged logic; the `destroyer(env, i)` off-by-one is already correct
  (`knifes[i]` failed, so destroy `[0, i)`)
- `init_philos` — assign `id` (1-based), `l_mtx`/`r_mtx`, the `env` back-pointer,
  `last_nom = env->times.start`, `hungry = 0`

Ordering matters: `times.start` must be set **before** `init_philos`, or every `last_nom`
starts at 0 and the monitor declares instant mass death.

### `philo/pthread_helper.c` (keep name)

- `destroyer(t_env *env, int counter)` — **drop the `e_msg` parameter entirely**; that
  unused parameter is one of the two `-Werror` failures. Callers now do
  `destroyer(env, i); return (ende("..."));`
- `start_threads` / `join_threads`

### `philo/main.c`

`main` becomes: `check_args` → `parse_args` → `init_env` → `init_knifes` →
`init_philos` → `start_threads` → `monitor` → `join_threads` → `destroyer` →
`return (0)`, with each failure returning 1. Also fix the `main.c:18` continuation-line
indentation (use **tabs**, not spaces, and keep `||` at the start of the new line).

---

## Step 7 — Makefile

`philo/Makefile`. Norm III.11 forbids wildcards — every source explicitly named. The
`$(SRCS:.c=.o)` substitution and the `%.o: %.c` pattern rule are both fine.

```make
NAME    = philo
CC      = cc
CFLAGS  = -Wall -Wextra -Werror -pthread
SRCS    = main.c args.c helprs.c ini_mini.c pthread_helper.c \
          routine.c monitor.c utils_time.c
OBJS    = $(SRCS:.c=.o)
HEADERS = philomilo.h func.h

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
```

Listing `$(HEADERS)` as a prerequisite means editing a header rebuilds what depends on
it, while `make` twice in a row still reports nothing to do — which is exactly the
"no unnecessary relinking" rule.

---

## Step 8 — README.md (graded, easy to forget)

Chapter VII requires `README.md` at the **root of the git repository** —
`/Users/arrilein/philosophers/README.md`, *not* inside `philo/`. In English, containing:

1. First line, italicised, verbatim shape:
   `*This project has been created as part of the 42 curriculum by stmuller.*`
2. `## Description` — goal and brief overview
3. `## Instructions` — compilation and how to run
4. `## Resources` — references **and a description of how AI was used, naming which
   tasks and which parts of the project**

That last clause is a literal subject requirement — worth writing honestly.

Optional: a `.gitignore` for `philo/*.o`, `philo/philo`, and the two PDFs (currently
untracked and cluttering `git status`).

---

## Verification

### Gate 1 — it builds and conforms
```sh
cd philo && make re && make          # 2nd must say "Nothing to be done for `all'."
norminette .                         # every file: OK!
```

### Gate 2 — argument handling (all must print an error and exit 1, none may crash)
```sh
./philo                    ./philo 5                  ./philo 5 800 200 200 7 9
./philo 0 800 200 200      ./philo -5 800 200 200     ./philo 201 800 200 200
./philo abc 800 200 200    ./philo 99999999999 800 200 200
```

### Gate 3 — behaviour
| Command | Expected |
|---|---|
| `./philo 1 800 200 200` | takes one fork, dies at ~800 ms |
| `./philo 5 800 200 200` | runs forever, nobody dies |
| `./philo 5 800 200 200 7` | stops on its own once all have eaten 7× |
| `./philo 4 410 200 200` | nobody dies (tight) |
| `./philo 2 400 200 200` | nobody dies |
| `./philo 200 800 200 200` | nobody dies |
| `./philo 4 310 200 100` | one dies |

### Gate 4 — the two rules peers actually test
```sh
./philo 4 310 200 100 | tail -3      # 'died' must be the LAST line, nothing after
```
And the `died` timestamp must land within **10 ms** of `last_nom + times.die`.

### Gate 5 — no data races (an explicit subject requirement)
```sh
cc -fsanitize=thread  -g -pthread *.c -o philo_tsan && ./philo 5 800 200 200 5
cc -fsanitize=address -g -pthread *.c -o philo_asan && ./philo 5 800 200 200 5
```
TSan must report zero races. On the Linux eval machines, cross-check with
`valgrind --tool=helgrind ./philo 5 800 200 200 5`.

---

## Out of scope

`philo_bonus/` (processes + semaphores). Per Chapter VIII it is only assessed if the
mandatory part is perfect, so it is strictly a follow-up.

---

## Appendix A — How to write `init_philos()`

*Appended 2026-09-21 against commit `f0a359f` ("parse and init"). Everything here was
checked on a scratch copy of your code, not guessed: it compiles with
`-Wall -Wextra -Werror`, the header, `func.h` and the new functions pass norminette, and
the output in A.6 is real output. The C blocks in this appendix use real tabs, exactly as
verified, so they are norminette-safe to paste.*

### A.1 — Names used here

Your code renamed these in commit `f0a359f`. The glossary near the top of this file has
since been updated to match; this table records what changed:

| Concept | Old name | Your code (used here) |
|---|---|---|
| times this philosopher has eaten | `hungry` | `philo->ate` |
| times each must eat (5th arg) | `must_nom` | `env->hungry` |
| stop flag / its mutex | `stop` / `stop_mtx` | `env->died` / `env->died_mtx` |
| number of philosophers | `philo_count` | `env->n_philo` |
| thread handle | `thread` | `philo->thread_id` |

`t_env` still carries `philo_count` as well, but nothing reads it — `n_philo` is the live
one. Deleting `philo_count` removes the ambiguity.

### A.2 — What `init_philos` is for

It runs **once**, after `init_knifes` and before any thread exists. Its job is to fill in
`philos[0 .. n_philo - 1]` so that each philosopher knows three things: who it is, which
two knifes it may pick up, and where the shared state lives. It allocates nothing and
initialises no mutex, so it cannot fail — hence `void`, not `int`.

It lives in `ini_mini.c` next to `init_env` and `init_knifes`. That file goes from 2
functions to 3; the Norm limit is 5.

### A.3 — Prerequisites, in order

**1. `philomilo.h`: `l_mtx` and `r_mtx` must become pointers.** This is the big one.

```c
	t_mutex		*r_mtx;
	t_mutex		*l_mtx;
```

Right now both are `t_mutex` *by value*, so every philosopher owns two private mutexes
that nobody else can see. Philosophers 1 and 2 are meant to contend for the same knife;
with private copies, each locks its own copy and both succeed at the same moment. The
knife is duplicated — exactly what the subject says the mutex exists to prevent (*"To
prevent philosophers from duplicating forks, you should protect each fork's state with a
mutex"*). Nor can you fix it by assigning `philos[i].l_mtx = knifes[i]`: POSIX makes
locking a *copy* of a mutex undefined behaviour. The only correct wiring is pointers into
the one shared `knifes[]`, so that two neighbours hold the **same address**.

It is also what makes the deadlock trick in Step 4 work: `if (philo->l_mtx < philo->r_mtx)`
compares positions inside `knifes[]`, which means nothing if they are private copies.

**2. `philomilo.h` line 54: one tab too many.** `t_times		times;` in `t_env` has two tabs
where its neighbours have one, and norminette reports `MISALIGNED_VAR_DECL`. Make it
`t_times	times;`.

**3. `func.h`: add the two missing prototypes.** `main.c:59` calls `init_philos`, which is
currently undeclared — one of the two compile errors:

```c
void	init_philos(t_env *env, t_philo *philos, t_mutex *knifes);
size_t	get_time_ms(void);
```

**4. `get_time_ms()` has to exist.** `init_philos` needs it to stamp the simulation start.
It is the first function of Step 3, and it belongs in a new `utils_time.c` because
`helprs.c` is already at the Norm's 5-function limit. It returns `size_t` to match your
`t_times` fields:

```c
size_t	get_time_ms(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
}
```

`<sys/time.h>` is already in `func.h`. Like every file, the new one needs the 42 header.

**5. `parsing` must reject 0 philosophers.** The wiring divides by `n_philo`
(`% env->n_philo`), so `n_philo >= 1` is a hard precondition. Today
`philotoi(argv[1]) > P_MAX || philotoi(argv[1]) == -1` lets `0` through. Because overflow
already returns `-1`, one condition covers both cases:

```c
	if (philotoi(argv[1]) < 1 || philotoi(argv[1]) > P_MAX)
```

**6. `arg_checker` loops forever on valid input** — so right now you cannot reach
`init_philos` at all. Verified: `./philo 5 800 200 200` was still running after 3 seconds.
`i` is never incremented, and even if it were, `i < argc -1` would skip the last argument.
The loop needs `i < argc`, with an `i++;` after the `if`.

**7. `main.c:59` is missing its `;`.** That is the second compile error, and also the
reason norminette **hangs** on `main.c` instead of reporting it. Once it is fixed,
norminette runs and reports five more errors in `main.c`, on lines 30, 35, 44 and 60 as
the file stands today — none of them related to `init_philos`.

### A.4 — The idea: wiring a round table

Philosophers are numbered 1..n but live at array index `i` = 0..n−1. Philosopher `i`
takes the knife at its own index as its left, and the knife at the next index as its
right:

```
l_mtx = &knifes[i]
r_mtx = &knifes[(i + 1) % n_philo]
```

For 5 philosophers:

| Philosopher (`id`) | `i` | `l_mtx` | `r_mtx` |
|---|---|---|---|
| 1 | 0 | `knifes[0]` | `knifes[1]` |
| 2 | 1 | `knifes[1]` | `knifes[2]` |
| 3 | 2 | `knifes[2]` | `knifes[3]` |
| 4 | 3 | `knifes[3]` | `knifes[4]` |
| 5 | 4 | `knifes[4]` | **`knifes[0]`** ← wraps |

```
               P1
         k0 /      \ k1
          P5        P2
         k4 \      / k2
             P4-k3-P3
```

Every knife appears exactly twice, in neighbouring rows: `knifes[1]` is shared by 1 and 2,
and `knifes[0]` by 5 and 1. That last pair is the subject's *"Philosopher number 1 sits
next to philosopher number number_of_philosophers"*.

**Why the `%`:** without it, philosopher 5's right knife would be `knifes[5]` — one past
the last initialised mutex. `% n_philo` wraps 5 back to 0 and closes the circle.

**One philosopher needs no special case here.** With `n_philo == 1`, `(0 + 1) % 1 == 0`,
so `l_mtx == r_mtx == &knifes[0]` — one knife, exactly as the subject says (*"If there is
only one philosopher, they will have access to just one fork"*). The special case belongs
in the **routine** (Step 4's `solo_philo`), which must never lock both: locking the same
default mutex twice from one thread self-deadlocks in practice, and strictly it is
undefined behaviour. `philo->l_mtx == philo->r_mtx` is a clean, self-documenting way to
detect it.

### A.5 — The function

```c
void	init_philos(t_env *env, t_philo *philos, t_mutex *knifes)
{
	int	i;

	env->times.start = get_time_ms();
	i = 0;
	while (i < env->n_philo)
	{
		philos[i].id = i + 1;
		philos[i].env = env;
		philos[i].times = env->times;
		philos[i].l_mtx = &knifes[i];
		philos[i].r_mtx = &knifes[(i + 1) % env->n_philo];
		philos[i].last_nom = env->times.start;
		philos[i].ate = 0;
		i++;
	}
}
```

15 lines, 1 variable, 3 parameters — well inside the Norm. Field by field:

| Field | Set to | Why |
|---|---|---|
| `id` | `i + 1` | The subject numbers philosophers from 1; the array starts at 0. It is printed in every log line. |
| `env` | `env` | The back-pointer: how the thread reaches `died`, the mutexes and `hungry`. |
| `times` | `env->times` | Your `t_philo` keeps its own copy of the timings. That is fine to keep: it is written here, before any thread exists, and only read afterwards, so there is no data race. The payoff is `philo->times.nom` instead of `philo->env->times.nom` — shorter lines under the 80-column rule. |
| `l_mtx` | `&knifes[i]` | See A.4. |
| `r_mtx` | `&knifes[(i + 1) % env->n_philo]` | See A.4. |
| `last_nom` | `env->times.start` | **Not 0, and not left unset.** The monitor computes `now - last_nom`; from 0 that is the milliseconds since 1970, so every philosopher is "dead" on the monitor's first sweep. Left unset, you get random deaths, because `philos[]` is an uninitialised stack array in `main`. |
| `ate` | `0` | Nobody has eaten yet. Same uninitialised-stack reason. |
| `thread_id` | *not touched* | It is an **output** of `pthread_create(&philos[i].thread_id, ...)`, filled in by the next step. |

**Why `start` is set on the first line, inside `init_philos`:** two things depend on it and
both must see the final value. `last_nom` must equal `start`, and each `times` copy must
carry `start` — copy before setting it, and every philosopher holds a stale `start`.
Setting it here, before the loop, makes both impossible to get wrong. This supersedes the
ordering warning in Step 6, since `init_philos` now owns it.

The signature keeps your call from `main.c:59`, `(env, philos, knifes)`. Note that
`init_env` has already stored the same two pointers in `env->philos` and `env->knifes`,
so `init_philos(t_env *env)` would work equally well. Your call.

### A.6 — Call it, then check it before writing any thread code

In `main`, stop ignoring the two return codes, then call it:

```c
	if (init_env(&env, knifes, philos))
		return (1);
	if (init_knifes(&env, knifes))
		return (1);
	init_philos(&env, philos, knifes);
```

`init_env` and `init_knifes` already return 1 on failure, but `main` currently carries on
regardless, with half-initialised mutexes.

**Then verify the wiring.** Drop this *temporary* function into `main.c` above `main`, and
call `debug_wiring(&env, knifes);` right after `init_philos`. Subtracting the array's base
from a pointer into it gives the index, and `%td` is the `printf` format for that
difference:

```c
void	debug_wiring(t_env *env, t_mutex *knifes)
{
	int	i;

	i = 0;
	while (i < env->n_philo)
	{
		printf("philo %d: l=knifes[%td] r=knifes[%td] ",
			env->philos[i].id, env->philos[i].l_mtx - knifes,
			env->philos[i].r_mtx - knifes);
		printf("last_nom-start=%zu ate=%d\n",
			env->philos[i].last_nom - env->times.start, env->philos[i].ate);
		i++;
	}
}
```

Build with `cc -Wall -Wextra -Werror *.c -o philo` from `philo/` (there is no Makefile
yet). This is the real output from the scratch build:

```
$ ./philo 5 800 200 200
philo 1: l=knifes[0] r=knifes[1] last_nom-start=0 ate=0
philo 2: l=knifes[1] r=knifes[2] last_nom-start=0 ate=0
philo 3: l=knifes[2] r=knifes[3] last_nom-start=0 ate=0
philo 4: l=knifes[3] r=knifes[4] last_nom-start=0 ate=0
philo 5: l=knifes[4] r=knifes[0] last_nom-start=0 ate=0

$ ./philo 1 800 200 200
philo 1: l=knifes[0] r=knifes[0] last_nom-start=0 ate=0

$ ./philo 200 800 200 200 | tail -1
philo 200: l=knifes[199] r=knifes[0] last_nom-start=0 ate=0

$ ./philo 0 800 200 200
Invalid Philo amount
```

What to look for: every `l` equals the previous line's `r`; the last philosopher's `r` is
`knifes[0]`; the solo run shows the same knife twice; and `last_nom-start` and `ate` are
all 0. **Delete `debug_wiring` before moving on** — it is test scaffolding, not project
code.

### A.7 — Questions an evaluator can ask about this function

- **Why are `l_mtx`/`r_mtx` pointers and not mutexes?** Neighbours must lock the *same*
  mutex. A copy is a different mutex, and POSIX makes locking a copied mutex undefined.
- **What does `% n_philo` do?** It wraps the last philosopher's right knife around to
  `knifes[0]`. That is what makes the table round.
- **What happens with one philosopher?** `l_mtx == r_mtx`: one knife, as the subject says.
  The routine detects that and never locks it twice.
- **Why is `last_nom` set to `start` and not 0?** The monitor measures `now - last_nom`.
  Measured from 0, everyone is instantly dead.
- **Why is there no mutex anywhere in `init_philos`?** No thread exists yet. POSIX lists
  `pthread_create` as a memory-synchronisation point, so everything written here is
  visible to the threads it starts.

### A.8 — Optional: a portability wrinkle in the header

`philomilo.h` declares `typedef struct s_env t_env;` on line 25, then declares it *again*
at the bottom as `typedef struct s_env { ... } t_env;`. Repeating a typedef is legal from
C11 on, so plain `cc` accepts it — but under `-std=c99` it is a hard error:
`redefinition of typedef 't_env' is a C11 feature`. That is harmless if the evaluation
machine builds with plain `cc`. The fix is to drop `typedef` and the trailing `t_env` from
the bottom definition; this was verified to compile under both standards and to pass
norminette:

```c
struct s_env
{
	t_mutex	print_mtx;
	...
	bool	died;
};
```

---

## Appendix B — Bugs outside `init_philos`

*Appended 2026-09-21 against commit `f0a359f`, using the same method as Appendix A: every
snippet was compiled with `-Wall -Wextra -Werror` and run through norminette on a scratch
copy, and the C blocks use real tabs. With Appendices A and B both applied, all seven
files pass norminette. Normal runs never exercise the failure paths, so the scratch build
also forced each mutex init to fail in turn and checked that nothing was left initialised
and no uninitialised mutex was ever destroyed; the results are in B.7.*

The sections are grouped by the function you edit, because several bugs share one fix.

### B.1 — `init_env`: three bugs, one rewrite

- **`died` is never set to `false`.** `t_env env` is an uninitialised local in `main`, so
  the stop flag starts as whatever was on the stack. Once threads exist, they may stop at
  once, or never.
- **`died_mtx` is never initialised.** Locking an uninitialised mutex is undefined
  behaviour, and the routine and monitor will lock it constantly.
- **The `||` short-circuit.** If `nom_mtx` fails, `print_mtx` is never initialised, yet
  `destroyer` destroys both — and destroying a mutex that was never initialised is
  undefined too. The continuation line is also where `ini_mini.c`'s three norminette
  errors come from.

Initialise one mutex at a time, and on failure destroy exactly the ones that already
succeeded:

```c
int	init_env(t_env *env, t_mutex *knifes, t_philo *philos)
{
	env->knifes = knifes;
	env->philos = philos;
	env->died = false;
	if (pthread_mutex_init(&env->print_mtx, NULL) != 0)
		return (ende("Mutex init failed(env)"));
	if (pthread_mutex_init(&env->nom_mtx, NULL) != 0)
	{
		pthread_mutex_destroy(&env->print_mtx);
		return (ende("Mutex init failed(env)"));
	}
	if (pthread_mutex_init(&env->died_mtx, NULL) != 0)
	{
		pthread_mutex_destroy(&env->print_mtx);
		pthread_mutex_destroy(&env->nom_mtx);
		return (ende("Mutex init failed(env)"));
	}
	return (0);
}
```

17 lines, no variables. With no continuation line left, the three norminette errors go
too. It no longer calls `destroyer` at all, and that's deliberate: `destroyer` destroys all
three env mutexes unconditionally, which is only correct once all three exist.

### B.2 — `destroyer`: the `-69` sentinel, and `died_mtx`

`if (counter == -69) return (0);` is meant to give a silent, successful cleanup. It can't,
because the loop above it consumes `counter`: `while (--counter >= 0)` always leaves it at
`-1` when it started at 0 or more, or one below the start when it started negative. It
equals `-69` only if you passed `-68`, and then zero knifes are destroyed. So **no call**
does what the end of the simulation needs — destroy all knifes and return 0. Verified:
calling `destroyer(&env, NULL, env.n_philo)` after a successful run makes the program exit
with **1**.

Use `e_msg` as the signal instead, where `NULL` means a normal cleanup, and destroy
`died_mtx` too:

```c
int	destroyer(t_env *env, char *e_msg, int counter)
{
	while (--counter >= 0)
		pthread_mutex_destroy(&env->knifes[counter]);
	pthread_mutex_destroy(&env->nom_mtx);
	pthread_mutex_destroy(&env->print_mtx);
	pthread_mutex_destroy(&env->died_mtx);
	if (!e_msg)
		return (0);
	return (ende(e_msg));
}
```

The two ways to call it:

| Situation | Call | Destroys | Returns |
|---|---|---|---|
| normal end of `main`, after `join_threads` | `destroyer(&env, NULL, env.n_philo)` | all knifes + the 3 env mutexes | `0` |
| `init_knifes` fails at knife `i` | `destroyer(env, "Mutex init failed(knifes)", i)` | knifes `[0, i)` + the 3 env mutexes | `1`, message printed |

The second row is what your `init_knifes` already does, so nothing changes there. This
also retires the magic number, which an evaluator would certainly ask about.

### B.3 — `parsing`: the 5th argument

`if (argc == 6 && philotoi(argv[5]) > 0) ... else env->hungry = -1;` treats every
non-positive result as "no limit". So `./philo 5 800 200 200 99999999999` — an overflow,
for which `philotoi` returns `-1` — silently becomes "eat forever" instead of an error,
and so does `0`.

Replace those four lines with:

```c
	env->hungry = -1;
	if (argc == 6)
	{
		env->hungry = philotoi(argv[5]);
		if (env->hungry == -1)
			return (ende("Invalid ARGs"));
	}
```

`is_legal` has already rejected signs and non-digits, so the only `-1` left is overflow.

That leaves `0` as a valid value. The subject stops the simulation once every philosopher
has eaten *at least* that many times, which with 0 is true before anyone moves. So handle
it in `main`, right after `parsing` and before anything is initialised — there is nothing
to clean up and nothing to print:

```c
	if (env.hungry == 0)
		return (0);
```

Rejecting `0` as invalid is also defensible at evaluation. Pick one and be ready to
justify it.

### B.4 — `ende`: the missing newline

Error messages are written without `\n`, so `Wrong argc` runs straight into the shell
prompt. Verified by the last byte the program writes: `c` before this fix, `\n` after.

```c
int	ende(char *e_msg)
{
	if (e_msg)
	{
		write(2, e_msg, ft_strlen(e_msg));
		write(2, "\n", 1);
	}
	return (1);
}
```

The newline sits inside the `if`, so a `NULL` message prints nothing at all.

### B.5 — `main.c`'s norminette errors, spelled out

A.3.7 listed these by line number only. The line numbers are as of commit `f0a359f` and
will shift as you edit:

| Line | Error | Fix |
|---|---|---|
| 30 | `NEWLINE_PRECEDES_FUNC` | Add an empty line between `arg_checker`'s `}` and `parsing` |
| 35 | `LINE_TOO_LONG` | Split the condition over two lines, as below |
| 44 | `TAB_INSTEAD_SPC` | Delete the stray tab after `return (0);` |
| 60 | `EMPTY_LINE_FUNCTION`, `SPACE_EMPTY_LINE` | The line holding only a tab before `main`'s `}`; it disappears with the A.6 changes |

The line-35 split that norminette accepts — operator at the start of the new line, and
the continuation indented one extra **tab**:

```c
	if (philotoi(argv[2]) <= 0 || philotoi(argv[3]) <= 0
		|| philotoi(argv[4]) <= 0)
		return (ende("Invalid ARGs"));
```

### B.6 — `main` with A and B applied

Putting A.6, B.2 and B.3 together, this is `main` until threads exist. It passes
norminette:

```c
int	main(int argc, char **argv)
{
	t_env		env;
	t_mutex		knifes[P_MAX];
	t_philo		philos[P_MAX];

	if (arg_checker(argc, argv))
		return (1);
	if (parsing(&env, argc, argv))
		return (1);
	if (env.hungry == 0)
		return (0);
	if (init_env(&env, knifes, philos))
		return (1);
	if (init_knifes(&env, knifes))
		return (1);
	init_philos(&env, philos, knifes);
	return (destroyer(&env, NULL, env.n_philo));
}
```

Steps 4–6 later insert `start_threads`, `monitor` and `join_threads` between
`init_philos` and the final `destroyer` call.

Check it with these; this is the scratch build's real behaviour:

| Command | Exit | Output (on stderr) |
|---|---|---|
| `./philo` | 1 | `Wrong argc` |
| `./philo 5 800 abc 200` | 1 | `Illegal ARG` |
| `./philo 0 800 200 200` | 1 | `Invalid Philo amount` |
| `./philo 5 99999999999 200 200` | 1 | `Invalid ARGs` |
| `./philo 5 800 200 200 99999999999` | 1 | `Invalid ARGs` |
| `./philo 5 800 200 200 0` | 0 | *(nothing)* |
| `./philo 5 800 200 200` | 0 | *(nothing yet; no threads)* |

### B.7 — What the failure injection showed

Each run of `./philo 5 800 200 200` made one mutex init fail, in order. "Left
initialised" counts mutexes that were never destroyed; "destroyed uninitialised" counts
`pthread_mutex_destroy` calls on a mutex whose init never succeeded. The harness was a
test-only shim on the scratch copy, not something to add to the project.

Your current code, which initialises only two env mutexes (`nom_mtx` first, then
`print_mtx`), so its knifes begin at the third init:

| Which init fails | Exit | Left initialised | Destroyed uninitialised |
|---|---|---|---|
| none — normal end | **1** ✗ | 0 | 0 |
| `nom_mtx` (1st) | 1 | 0 | **2** ✗ |
| `print_mtx` (2nd) | 1 | 0 | **1** ✗ |
| any knife | 1 | 0 | 0 |

With B.1 and B.2:

| Which init fails | Exit | Left initialised | Destroyed uninitialised |
|---|---|---|---|
| none — normal end | 0 | 0 | 0 |
| `print_mtx`, `nom_mtx` or `died_mtx` | 1 | 0 | 0 |
| `knifes[0]`, `knifes[2]` or `knifes[4]` | 1 | 0 | 0 |

---

## Appendix C — Answers to `questions.txt`

*Appended 2026-09-22, against commit `6f29fd2`. Both answers were checked on a scratch
copy; `philo/` was not touched.*

### C.1 — Why lock `nom_mtx` for `last_nom` and `ate`, if they belong to one philo?

Because what matters is how many **threads** touch the memory, not who owns it. The
philosopher's thread writes these two fields; the monitor (Step 5) reads them from its own
thread. Two threads, at least one writing, nothing ordering them: that is a data race. The
subject forbids data races, and C makes them undefined behaviour — the compiler may keep
`last_nom` in a register or reorder the write, so the monitor can act on a stale value.

You're right that data used by only one thread needs no lock. That's why the routine can
read `philo->ate` unlocked (it's the only writer), and why nothing needs the lock *yet*:
the monitor doesn't exist.

**Both sides must lock.** The writer's unlock, followed by the monitor's lock, is what
guarantees the monitor sees the new value. One side alone guarantees nothing.

Measured with ThreadSanitizer, `./philo 5 800 200 200 3`, your `mahlzeit` plus a test
monitor:

| writer (`mahlzeit`) | reader (monitor) | data races |
|---|---|---|
| locked (your code) | locked | 0 |
| unlocked | locked | 2 |
| locked | unlocked | 2 |

The two races are exactly `routines.c:27` (`last_nom`) and `routines.c:28` (`ate++`).

### C.2 — Why is the `if` in `grab_em()` enough to prevent deadlock?

It makes everyone take the lower-addressed knife first. `knifes` is one array, so a lower
address means a lower index. Only philosopher 5, whose right knife wraps around to
`knifes[0]`, takes the `else` branch — so it takes `knifes[0]` first, just like
philosopher 1.

A deadlock needs a circle in which each philosopher holds one knife and waits for the
next one's. Under the rule, everyone waits for a knife with a *higher* address than the
one they hold. Going round the circle, the addresses would have to keep rising and still
arrive back at the start — K₁ < K₂ < … < K₁ — which is impossible. No circle, no
deadlock.

Concretely: without the `if`, all five grab their left knife and wait forever for the
right one. With it, 1 and 5 compete for `knifes[0]` first; the loser holds nothing, so the
circle has a gap.

Measured in the worst case (20 ms between first and second knife, no start stagger),
`./philo 5 800 200 200 3`, 3 runs each:

| `grab_em` | result |
|---|---|
| without the `if` | hung 3/3 — each took one knife, then all blocked in `pthread_mutex_lock` |
| with the `if` (yours) | finished 3/3, all 15 meals |

Comparing pointers with `<` is only defined inside one array — which `knifes` is.

What the `if` does **not** do:

- **One philosopher:** `l_mtx == r_mtx`, so the `else` locks the same mutex twice and
  hangs — `./philo 1 800 200 200` never ends. It needs its own path; see `review.md` R2.
- **Starvation:** it guarantees that someone can eat, not that everyone eats in time. Odd
  counts currently starve philosophers 1 and 5; see `review.md` R4.
