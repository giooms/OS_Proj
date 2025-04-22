# Challenge 1: Anti-Cheat

Nowadays, many games use kernel-level anti-cheat systems to prevent cheating. The reason why these anti-cheat systems are implemented in the kernel is that they can monitor and control drivers, hardware, virtual memory of user-space processes or of the kernel.

These anti-cheat are too complicated for us (especially for your first challenge). Therefore, the anti-cheat system you will implement could in fact have been implemented in user-space, but you will do it in the kernel using eBPF instead.

## Description

The game you are going to protect is a very simple terminal-based hangman game.
```
You guessed e and revealed 0 characters!

**************************************

*****************************\\|//**
**You have 3 guesses left     \|/
**                             |
**                            ()
**                           /||\
**                            //
** _ k i _ i d i
*************************
Guessed letters: a - - d e - - - i - k - - - - p - - - - - - - - - -

Enter a letter here:
```

The rules are simple: the player has to guess a word letter by letter. If the letter is in the word, it is displayed. If the letter is not in the word, an additional part of the hangman is drawn (in the implementation, the hangman is always drawn, but whatever).

The hangman has 6 parts: head, body, left arm, right arm, left leg, right leg. Therefore, the player has 6 chances to guess the word.

The game developer is worried that some players might cheat by modifying the initial number of guesses they have, since it is stored in a file called `guesses` and players could try to modify it.

Your task is to check that the number of guesses at the beginning of the game is 6. If it is not, you should kill the game process.

## Setup
Download the files for this challenge using:
```
$ wget --no-check-certificate https://people.montefiore.uliege.be/~gain/courses/info0940/asset/anticheat.tar.gz
$ tar -xzvf anticheat.tar.gz
```
The game is located in `anticheat/hangman`. The original source code can be found at: https://github.com/luker983/hangman/tree/master. We modified it to add the guesses file and to simplify the code.

You can compile it using the Makefile provided (simply run `make` within the `hangman` directory). Then you can run the game using:
```
$ ./hangman
```
You can try cheating by modifying the `guesses` file:
```
$ echo 100 > guesses
```
Then run the game again. You should now have 100 guesses instead of 6.

Inside `anticheat/src`, you will find the same template as in tutorial 3. Use it to implement the anti-cheat system.

## What you need to do
What is expected of you is to implement a uprobe that hooks the `read_guesses` function of the game. This function reads the number of guesses from the `guesses` file.

If it reads a number different from 6, you should kill the game process.

This is an example of what it will look like when you try to run the game with a modified `guesses` file:

```
$ ./hangman
Okay, I will play with you. I have a word in mind.
[1]    96003 killed     ./hangman
```

This challenge is quite simple, don’t be surprised if your solution is very short.