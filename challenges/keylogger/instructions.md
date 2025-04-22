# Challenge 5: Keylogger

Ever wondered what your keyboard is whispering to your OS? With eBPF, you can sneakily listen in—without crashing the system or setting off alarm bells!

For this challenge, you will create a stealthy eBPF-based keylogger that monitors specific keystrokes entered by the user and displays the captured input as a message.

This challenge will require you to efficiently manage input events, use eBPF maps and perf buffers, and ensure smooth real-time logging.

## Description

For this challenge, the files to download are simply the template for your code:
```
$ wget --no-check-certificate https://people.montefiore.uliege.be/~gain/courses/info0940/asset/keylogger.tar.gz
$ tar -xzvf keylogger.tar.gz
```
Your task is to develop a keylogger that tracks and stores specific keystrokes entered by the user, then displays all the saved keys when the user presses the `ENTER` key. All the keys must be shown by the eBPF program using a **perf buffer**.

## What you need to do

Your task is to write an eBPF program that captures and processes specific keystrokes entered by the user. The program must efficiently track keystrokes and store them in a data structure, ultimately constructing a buffered message of fixed size : **32 bytes**.

The keystrokes to handle are:

- Alphabetic characters: both uppercase (`A-Z`) and lowercase (`a-z`)
- Numeric characters: `0-9`
- Special symbols: `,.-=` and space
- ENTER (`↵`): Displays the buffered message via the **perf buffer**
- TAB (`↹`): Clears the entire buffered message
- BACKSPACE (`⌫`): Removes the last recorded keystroke
- SHIFT/CAPSLOCK (`⇪`): Toggles the character case

Your program should maintain a buffered message, updating it dynamically based on user input. When the `TAB` key is pressed, the buffer should be completely cleared, and when `BACKSPACE` is pressed, only the last saved character should be removed—simulating a real-time text editor effect. Note that it has no effect if the buffered message size is 0. Pressing `ENTER` should display the message on stdout. The program must also correctly handle both uppercase and lowercase characters (via `SHIFT` and `CAPSLOCK`). Any other keystrokes must be ignored.

When the buffered message reaches its maximum size (32 bytes), the oldest characters should be replaced, implementing a circular buffer behavior.

### Important

- Consider that by default, `CAPSLOCK` is never enabled.
Handling repeated keystrokes (when a key is held down during several seconds) is not required.
- For testing, you need to enter the keys directly in the virtual machine rather than in the SSH client on your terminal.
- Verify that your keyboard layout is the same on both your - SSH client and the virtual machine. To ensure consistency, use `sudo loadkeys be` or `sudo loadkeys us` in both environments.

As example, the figure below illustrates the behavior of the keylogger.

![keylogger](../_images/keylogger.png)

Regarding the small notes:

- *1: “HelLo” was initially typed. Then, the `BACKSPACE` key was used to remove the ‘L’, and ‘l’ was typed instead. As a result, the final output displayed is “Hello”, where ‘L’ has been deleted and replaced by ‘l’.
- *2: The message “It is a circular buffer” was deleted using several backspaces.
- *3: After pressing `ENTER`, two solutions can be considered: (1) either truncating the message until the end (e.g., the ‘E’ is also deleted) or (2) tracking the deletion to keep the last character(s). Both approaches are valid, and choosing one over the other will not affect your mark.