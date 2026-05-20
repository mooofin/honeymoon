/*
 * Bootstrapper. Wires Buffer + Terminal -> Kernel.
 */
#include <cstdio>
#include "editor.hpp"
#include "buffer.hpp"
#include "terminal.hpp"

int main(int argc, char* argv[]) {
    using Buf = honeymoon::mem::GapBuffer<char>;
    using Term = honeymoon::driver::Terminal;
    honeymoon::kernel::Editor<Buf, Term> editor;
    
    if (argc >= 2) editor.open(argv[1]);
    editor.run();
    return 0;
}
