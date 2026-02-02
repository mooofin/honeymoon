# Honeymoon

**FUCK VIM.**
And Honeymoon doesn’t run a Lisp interpreter that eats 4GB of RAM on startup.

A minimal, Emacs-inspired terminal text editor written in **C++20**.

Fast as fuck 

## Features


<p align="center">
  <img src="https://github.com/user-attachments/assets/12d31b00-0e00-4a1f-a94c-39ae24d4250f" alt="honeymoon" />
</p>

**Visual Mode**

- `Ctrl-Space` → mark  
- move → expand selection  
- `Ctrl-W` → cut  

_Just like Emacs, but without the pinky pain._  
_(Okay, maybe some pinky pain.)_


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
<p align="center">
  <img src="https://i.pinimg.com/originals/18/7b/6a/187b6a6387bb1cd2cd3a7a786d05ea7a.gif" alt="gif">
</p>


## The Update



You can now map `Ctrl-Alt-Shift-Super-P` to `quit` if that makes you feel "productive." 

I replaced the O(1) map lookups with tree traversals . I hope you're happy.
