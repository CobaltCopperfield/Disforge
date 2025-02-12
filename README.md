# Disforge
\documentclass{article}
\usepackage{listings}
\usepackage{hyperref}
\usepackage{enumitem}
\usepackage{xcolor}

\title{x86 Disassembler Documentation}
\author{}
\date{}

\begin{document}

\maketitle

\section{Overview}
A lightweight x86 instruction disassembler written in C that converts machine code bytes into human-readable assembly instructions. The disassembler supports common 32-bit x86 instructions and addressing modes.

\section{Features}
\begin{itemize}
    \item Decodes common x86 instructions including:
    \begin{itemize}
        \item MOV (register, memory, and immediate variants)
        \item Arithmetic operations (ADD, SUB, MUL, DIV, etc.)
        \item Control flow (JMP, CALL, RET, conditional jumps)
        \item Stack operations (PUSH, POP)
        \item Bit manipulation (AND, OR, XOR, shifts)
        \item String operations (MOVSB, STOSB, etc.)
        \item Sign/zero extension (MOVZX, MOVSX)
    \end{itemize}
    \item Supports multiple addressing modes:
    \begin{itemize}
        \item Register-direct addressing
        \item Memory addressing with displacement
        \item SIB (Scale-Index-Base) addressing
    \end{itemize}
    \item Provides offset information for each instruction
    \item Handles instruction prefixes (LOCK, REP, REPNZ)
\end{itemize}

\section{Building}
To compile the disassembler, use a C compiler such as GCC:

\begin{lstlisting}[language=bash]
gcc -o disassembler disassembler.c
\end{lstlisting}

\section{Usage}
The disassembler can be used in two ways:

\subsection{Built-in Test Mode (Default)}
\begin{lstlisting}[language=bash]
./disassembler
\end{lstlisting}
This will disassemble a built-in set of test instructions.

\subsection{File Input Mode}
Requires uncommenting the alternative main function:
\begin{lstlisting}[language=bash]
./disassembler <machine_code_file>
\end{lstlisting}
This will disassemble machine code from the specified binary file.

\section{Output Format}
The disassembler outputs each instruction in the following format:
\begin{verbatim}
offset: instruction operands
\end{verbatim}

Example output:
\begin{verbatim}
0000: NOP
0001: MOV EAX, 0x12345678
0006: MOV ECX, 0x90ABCDEF
000b: ADD EAX, ECX
\end{verbatim}

\section{Limitations}
\begin{itemize}
    \item Only supports 32-bit x86 instructions
    \item Limited instruction set coverage
    \item No support for floating-point instructions
    \item No support for MMX/SSE instructions
    \item Only handles basic prefixes
\end{itemize}

\section{Implementation Details}
The disassembler works by:
\begin{enumerate}
    \item Reading machine code bytes sequentially
    \item Identifying instruction opcodes
    \item Decoding ModR/M and SIB bytes when present
    \item Handling immediate values and displacements
    \item Formatting the output in AT\&T syntax
\end{enumerate}

\subsection{Key Functions}
\begin{itemize}
    \item \texttt{disassemble()}: Main disassembly routine
    \item \texttt{decode\_rm\_operand()}: Decodes ModR/M addressing modes
    \item \texttt{print\_reg()}: Prints register names
    \item \texttt{print\_condition()}: Prints condition codes for conditional jumps
\end{itemize}

\section{Contributing}
Feel free to contribute by:
\begin{itemize}
    \item Adding support for more instructions
    \item Improving error handling
    \item Adding support for different syntax styles
    \item Enhancing documentation
    \item Fixing bugs
\end{itemize}

\section{License}
This project is open source and available under the MIT License.

\section{Notes}
This is a basic disassembler intended for educational purposes. For production use, consider using established disassembly frameworks like Capstone or LLVM.

\end{document}
