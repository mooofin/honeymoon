# Honeymoon

**FUCK VIM.**
And Honeymoon doesn’t run a Lisp interpreter that eats 4GB of RAM on startup.

A minimal, Emacs-inspired terminal text editor written in **C++20**.

Fast as fuck 

## Features


* **Visual Mode**
  `Ctrl-Space` to mark. Move. `Ctrl-W` to cut.
  Just like Emacs, but without the pinky pain.
  (Okay, maybe some pinky pain.)




## Architecture

The code is split into three namespaces:

* `honeymoon::kernel`
  Core editor logic.

* `honeymoon::driver`
   Raw mode, TTY I/O, screen rendering.
 

* `honeymoon::mem`
  A templated gap buffer implementation.




## Build

You need a C++20 compiler and `make`.

* Linux: you’re fine.
* Windows: WSL or sm 


```bash
make
./honeymoon filename.txt
```



## Controls

If you know Vim, I’m sorry.


