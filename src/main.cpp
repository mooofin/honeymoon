/*
 * Bootstrapper. Wires Buffer + Terminal -> Kernel.
 */
#include <iostream>
#include "editor.hpp"
#include "buffer.hpp"
#include "terminal.hpp"

int main(int argc, char* argv[]) {
    using Buf = honeymoon::mem::GapBuffer<char>;
    using Term = honeymoon::driver::Terminal;
    honeymoon::kernel::Editor<Buf, Term> editor;
    
    if (argc >= 2) editor.open(argv[1]);
    try { editor.run(); }
    catch (const std::exception& e) { std::cerr << "Panic: " << e.what() << std::endl; return 1; }
    return 0;
}
