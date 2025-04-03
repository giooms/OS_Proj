# Operating Systems - Project 1 (Challenges)

## Overview

This repository serves as the central hub for projects related to the Operating Systems (INFO-0940) class at the University of Liège (ULiège). It contains the 5 challenges relative to project 1 of the course.

## Project Structure

- **anticheat/**: Contains the files and code for the anti-cheat challenge, which involves implementing a kernel-level anti-cheat system using eBPF for a simple hangman game.
- **seccomp/**: Houses implementation and examples of secure computing mode (seccomp) for system call filtering and sandboxing techniques.
- **forkbomb/**: Implements an eBPF-based solution to detect and prevent fork bombs by monitoring process creation patterns and terminating processes that exhibit fork bomb behavior.
- **page_faults/**: Contains materials and code related to the page fault handling mechanisms, exploring memory management concepts in operating systems.
- **keylogger/**: Implements a keylogger functionality with a circular buffer (32 bytes). The buffer is printed out on stdout (via a perf buffer) upon press on the `Enter` key.

## Authors

- @martinDengis
- @giooms
