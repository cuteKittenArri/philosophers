NAME := philo
CC := cc
CFLAGS := -Wall -Werror -Wextra -MMD -MP -g
SRCS := main.c ini_mini.c raidbots.c routines.c helprs.c pthread_helprs.c time_helprs.c
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)
HEAD := func.h philomilo.h

all: $(NAME)
	
$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

clean:
	rm $(OBJS) $(DEPS)
	
fclean: clean
	rm $(NAME)
	
re: clean all

.PHONY: all clean fclean re

-include $(DEPS)
