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

Every identifier in this project is built from this vocabulary. **Do not import
alternative words** (no `eat`, `meal`, `fork`, `fed`, `full`) from the subject PDF or from
conventional 42 style, even when the subject's own argument names use them.

| Concept | Name | Lives in |
|---|---|---|
| a fork | `knifes[]` | `t_env` |
| this philosopher's two forks | `l_mtx`, `r_mtx` | `t_philo` |
| the act of eating | `nom()` | `routine.c` |
| how long eating takes | `times.nom` | `t_env.times` |
| when this philosopher last ate | `last_nom` | `t_philo` |
| how many times they have eaten | `hungry` | `t_philo` |
| how many times they must eat | `must_nom` | `t_env` |
| mutex guarding `last_nom` + `hungry` | `nom_mtx` | `t_env` |
| "have they all eaten enough?" | `check_all_nommed()` | `monitor.c` |

The subject calls the optional 5th argument
`number_of_times_each_philosopher_must_eat` — that is the *subject's* word, and it stops
at the parser. Inside the code it is `must_nom`, because the counter it is compared
against is `hungry` and the verb is `nom`.

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
