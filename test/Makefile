# SPDX-License-Identifier: AGPL-3.0-only
# Masaustunde kosan birim testler — kart, ekran ve sensor gerekmez.
BAYRAK = -std=c++17 -Wall -Wextra -Wpedantic -Werror -O1 -g
CEK    = ../firmware/orientation_cube/src

.PHONY: all temiz
all: kos_yonelim kos_kup3b

kos_yonelim: t_yonelim
	@./t_yonelim
kos_kup3b: t_kup3b
	@./t_kup3b

t_yonelim: test_yonelim.cpp $(CEK)/yonelim.cpp
	@g++ $(BAYRAK) -o $@ $^ -lm
t_kup3b: test_kup3b.cpp $(CEK)/kup3b.cpp
	@g++ $(BAYRAK) -o $@ $^ -lm

temiz:
	@rm -f t_yonelim t_kup3b
