# skibidi-emu
#### Video Demo:  <URL HERE>
#### Description: Chip-8 interpreter written in C using SDL2 as an abstraction layer for IO.
#### USAGE: From command line run {path to compiled skibidi-emu} {rom path} {rom type}

Note: roms must be Chip-8 compatible. rom type will usually be normal, but roms designed for the ETI 660 computer will not work unless rom type is "ETI"

#### The Source
The basis of this chip-8 interpreter is a well known document (at least as far as Chip-8 is concerned) - "Cowgod's Chip-8 Technical Reference v1.0" currently available at http://devernay.free.fr/hacks/chip8/C8TECH10.HTM \
This document describes the technical requirements to run a Chip-8 program.
#### The Spec
Chip-8 computers have 4096 bytes of memory where program data is stored, 16 1-byte registers (denoted Vx where x is the index of the register), a 2-byte register called I, a 1-byte each sound timer and delay timer register that decrement at 60Hz, the 2-byte (as it has to be able to index 0 to 4095) program counter, the stack which consists of 16 2-byte values to store addresses, and the stack pointer that keeps track of the index of the top of the stack.\
Typically, there would be 16 keys for input, which I have mapped to keys on a modern computer (qwerty) keyboard as shown below.
#### KEYMAP:
    KEYBOARD        =       INPUT
    1  2  3  4      =       1  2  3  C
    q  w  e  r      =       4  5  6  D
    a  s  d  f      =       7  8  9  E
    z  c  v  b      =       A  0  B  F
Chip-8 computers had a 64x32 monochrome display, and builtin capability to display sprites for all of the hexidecimal characters, 0 through F. Because it is monochrome and I'm trying to use less memory, 64 bits across and 32 lines of these bits really is equivalent to 32 lines of 8 bytes rather than something like a bool array[64][32], which would take up 8 times the memory as bools take up 1 byte despite only needing 1 bit due to the way memory addresses are mapped. In one part or another of the code, the conversion from a byte to true or false pixels would need to be made, so in the interest of keeping the actual Chip-8 portion of the code more memory compact, it is done here.\
\
With these details in mind, I set to writing an interpreter with the consideration of memory and processing limitations of Chip-8 computers.
In an effort to maintain some level of portability and to reduce memory usage where possible, there were no standard C ints involved with the CPU portion of the code (except perhaps some introduced by SDL which was required for the Fx0A instruction which required a key input), instead using more the portable uint8_t or uint16_t. In an effort to reduce memory usage, wherever possible no new variables were declared, instead opting to loop based on the copy of operation codes (op codes) that is passed when calling a function with the op code as an argument. Finally, I resolved to wherever possible not use division or modulo operators as they are considered quite costly compared to integer addition, multiplication and subtraction.\
#### Operations
There are 34 different instructions based on op code in my implementation, these op codes are provided in their hexidecimal representation, consisting of 2 bytes (4 nibbles) - 0xNNNN. The highest half-byte generally determines the type of instruction, for example 0xDXXX defines draw instruction with the other 3 half-bytes existing as arguments to the instruction that need to be decoded. 
I will now walk through some code examples.\
Starting off easy, the logic of the loop that executes instructions:
#### Executing Instructions:
    if (pc > 0xFFE) {
            printf("Program counter reached the end of the ROM.\n");
            goto err_close;
        }

        do_op((memory[pc] << 8) + memory[pc + 1]);
        pc += 2;
        
This code snippet is called repeatedly, which checks for a valid program counter value, pc, and breaks if it is out of range (more on this later). Then, calls do_op with the 2 bytes located at index pc in memory, and the program counter is incremented by 2 so that the next instruction can be executed. you may note that the first byte is bitshifted left 8 times, which makes room for the next byte to make up the op code.
#### do_op()
    void do_op(uint16_t op) {
       op_0xnxxx[(op & 0xF000) >> 12](op);
    }
the do_op definition looks relatively simple, however it calls op_0xnxxx[]() - an array of function pointers. This may (understandably) look look strange, but it was an optimisation I came up with based off of hashing like one might do for a dictionary or trie. Recall that the op code for draw was 0xDXXX and that my implementation has 34 different op code functions. If there was an arbitrary op code, say 0xD123 (this decodes to the pseudocode: draw the next 3 bytes that are stored starting at address I to the display at coordinate V[1], V[2]), if you stored a pointer to the draw function operator at index 0xD of the array of function pointers and passed it the operation code you can quickly call the correct function, as opposed to a 34 value switch statement or if ... else statement. Therefore, we jump one level deeper:

    void(*op_0xnxxx[16])() = {&op_0x0xxx, &JP, &CALL, &SEVX, &SNEVX, &SEVXVY, &LDVX, &ADD, &op_0x8xxx, &SNEVXVY, &LDI, &JPV0, &RND, &DRW, &op_0xExxx, &op_0xFxxx};

Here we define an array of 16 void pointers to functions and initialize it with the addresses of the cpu functions, or in the case of op codes starting with 0x0, 0x8, 0xE or 0xF, functions structured as if ... else statements to execute the appropriate instruction except 0x8, which is structured similarly to 0xnxxx.

    void(*op_0x8xyn[16])() = { &LDVXVY, &OR, &AND, &XOR, &ADDCARRY, &SUB, &SHR, &SUBN, &PASS, &PASS, &PASS, &PASS, &PASS, &PASS, &SHL, &PASS};

This was done similarly for the 9 instructions with op codes starting with 0x8, which can be accessed by their lowest half-byte, however as an incorrectly formed Chip-8 program may attempt to pass an op code with 8, 9, A, B, C, D or F as their lowest half-byte, I created the PASS function which does nothing and returns.\
#### Some Instructions
    void JP(uint16_t op) {
	    pc = (op & 0x0FFF) - 2;
    }
This instruction is the jump instruction, it simply sets the program counter to the lower 3 half-bytes using the bitwise AND operator and decrements it by 2 (to account for the increment by 2 after every call to do_op()).

    void ADDCARRY(uint16_t op) {
        V[(op & 0x0F00) >> 8] += V[(op & 0x00F0) >> 4];
        if (V[(op & 0x0F00) >> 8] < V[(op & 0x00F0) >> 4]) {
            V[0xF] = 0x01;
        }
        else {
	    	V[0xF] = 0;
	    }
    }
This is the add with carry instruction. the op code for this instruction takes the form 0x8XY4, which adds the values of V[X] and V[Y] and sets V[F] to 1 if the result overflows (>255 for individual bytes). Decoding for the index of the array of registers V is done with right bitshifts by 8 and 4 for X and Y and the result is stored in the V[X] register. C seems to (I could be wrong) typically evaluate an expression like V[X] + V[Y] Its actual value I.e. even though this is uint8_t addition and the max value is 255 for an unsigned 8-bit int, if you were to add (uint8_t) 255 and (uint8_t) 255, C seems to calculate that the result is 510, but when storing it, it only stores the low byte of this result, 254 (510-256, or more likely 0x1FE & 0xFF = 0xFE). This means that if V[X] is less than V[Y] after the addition, it has overflowed and you should set the carry bit in V[F]. As a side note, I have realised in writing this that the ADDCARRY function could be rewritten as:

    void ADDCARRY(uint16_t op) {
        if ((V[(op & 0x0F00) >> 8] += V[(op & 0x00F0) >> 4]) < V[(op & 0x00F0) >> 4]) {
            V[0xF] = 0x01;
        }
        else {
	    	V[0xF] = 0;
	    }
    }
Visual studio does yell at you for this and it would appear that C casts uint8_t to a 4 byte type for the purposes of logical operations given the error it gave me. I refrained from changing it in my source on the off chance it does actually break something, although I'm quite confident it won't. I was curious so I investigated and decided to write a brief appendix for this question (see appendix 1).
Next and Finally, we will take a brief look at my implementation of the DRW instruction

    void DRW(uint16_t op) {
        V[0xF] = 0;
    #define COORD_X (V[(op & 0x0F00) >> 8] >> 3)
    #define COORD_Y (V[(op & 0x00F0) >> 4])
    #define N (op & 0x000F)
        while (N) {
            op--;
            if (display[COORD_X & 0x7][(COORD_Y + N) & 0x1f] & (memory[I + N] >> (V[(op & 0x0F00) >> 8] - (COORD_X << 3)))) {
                V[0xF] = 0x01;
            }
            display[COORD_X & 0x7][(COORD_Y + N) & 0x1f] ^= memory[I + N] >> (V[(op & 0x0F00) >> 8] - (COORD_X << 3));
            // & with 0x07 to constrain to 3 bits for wrapping
            if (display[(COORD_X + 1) & 0x07][(COORD_Y + N) & 0x1f] & (memory[I + N] << (8 - (V[(op & 0x0F00) >> 8] - (COORD_X << 3))))) {
                V[0xF] = 0x01;
            }
            display[(COORD_X + 1) & 0x07][(COORD_Y + N) & 0x1f] ^= memory[I + N] << (8 - (V[(op & 0x0F00) >> 8] - (COORD_X << 3)));
        }
    #undef COORD_X
    #undef COORD_Y
    #undef N
    }

This one is a little bit harder to follow. First, V[F], which is typically used as a flag for various instructions. Then I decode the coordinates and number of lines of the sprite to draw from the op code and use #define so the preprocessor can substitute them for me (this was partially an excuse for me to use #defines). Using a const uint8_t would be faster because it will likely result in many fewer instructions, but I made an effort to constrain memory allocations as if it were really a computer with limited memory, perhaps foolishly, so I decided against this.\
While the lowest half-byte of the op code copy that got passed to the function is not 0, the op code copy is decremented and used to offset the Y coordinate. Line by line the if the high bits of a sprite that we are drawing to the screen overlap with the high bits in display[][], (done by bitwise anding the bytes together), V[F] is set to 1. Then the sprite is XORed into display.\
 Because of the decision to store display data in an 8x32 array, there are actually 2 separate very similar statements here to deal with the fact that coordinates may not line up with a byte, i.e. if V[X] = 4, we need to display the high half-byte of each line of the sprite in display[0][Y] and the low half-byte in display[1][Y], i.e. the byte of the sprite we are dealing with would need to be bitshifted right 4 times to align it correctly in display[0][Y] and bitshifted left 4 times to correctly align in display[1][Y].\
 the indices used to access display are bitwise anded with 0x7 (7) and 0x1F (31) to ensure that the sprites can correctly wrap around to the opposite side of the screen, i.e. a sprite drawn at 62, 30 (bottom right) will wrap around and be drawn in the top left corner. it will also be drawn partially in the bottom left and top right corners. 

#### Looping in main()
    int main(int argc, char* argv[])
    {
    #if (TEST == 1)
        //the tests
    #else
        //handle args
        //initialize the memory, window, SDL subsystems etc.
        //load rom
        struct timespec frame_start = { 0, 0 };
        struct timespec frame_end = { 0, 0 };
        bool quit = false;
        while (quit == false) {
            //get frame_start time
            //decrement timers
            while (frame_end-frame_start < 1/60) {
                //poll event queue and handle events

                //do ops (you remember this, right?)

                if (instruction_type == draw) {
                    do{
                        //frame_end = now
                    } while (frame_end - frame_start < 1/60);
                    //draw frame
                    //break loop
                }
                //frame_end = now
            }	
        }
        //close SDL subsystems etc.
        return 0;
    err_close:
        return 1;
    #endif
    }

above is a very simplified version of the loops within the main() function with most of the logic substituted for pseudocode. To set a framerate (which can be altered at the top of the source, but I believe is supposed to be 60 frames/s) two timespec structs are defined, called frame_start and frame_end. A bool to break the loop called quit is defined as false, but will be set to true in the case that an event from the queue that we poll causes the window to close. We enter into the actual loop that prevents us from falling out of the bottom, destroying the window and returning from main. the start of every iteration of the while(quit==false) loop that defines a frame, we record the start time and decrement both of the timer registers if necessary. The next important thing that happens is entry into another loop that runs until 1/60th of a second has passed. in this loop, the SDL event queue is polled once, which handles keyboard input, used to interact with Chip-8 programs and detects events that cause the window to quit. Then one operation is done with do_op(). If the previous instruction wasn't a draw instruction frame_end is assigned the current time and the end of one iteration of the while (end-start < 1/60) loop has ended. If the last instruction executed was a draw instruction (updated the display), we loop, repeatedly getting the current time and assigning it to frame_end, checking if it has been 1/60th of a second yet and if it has, we convert the data in display into a series of pixels that is then drawn to the screen. Then the timer loop breaks, going back out to the while (quit==false) loop's scope, the end of the loop iteration is reached and the loop begins again, which comprises one frame.\

#### Flickering Display
As a quick note: if one runs a program with the Chip-8 interpreter, flickering will be seen and this is actually not a bug per se. I made the decision to draw a new frame after every draw instruction, this is because if not then, how would you know when? Perhaps a counter and after some arbitrary number of instructions have been executed, you render the data condained in display to the screen, but this would likely lead to variable program speeds and still does not solve the problem of flickering.
You may then ask, why does Chip-8 flicker? The answer is actually how it handles display by design. Say, for example, you were running pong. How do you update the position of one of the paddles if you move it up one pixel? A simple answer may be simply decrement (generally negative is upwards or leftwards) the register that has the y coordinate of the paddle and draw it again, but the spec for the draw instruction calls for a sprite to be XORed onto the existing display, you may wonder what this means for our paddle? What it means is that rather than moving the paddle up by 1 pixel, you have XORed the paddle with another paddle sprite 1 pixel higher, resulting in 1 pixel on at the top of the paddle and the bottom pixel of the paddle's previous location also being on, with the remainder off. The truth is that you must first draw the paddle where it currently is to erase it from the screen, then decrement it's Y coordinate, then you can draw it to the screen once again and this disappearance causes the flicker, unavoidably. Even if one were to execute the relatively (although not in modern computer terms) expensive CLS instruction to clear the screen, sure you could immediately draw that one paddle correctly without a draw to erase it, but you'd erase the ball and the other paddle and the scores, which would also need to be redrawn.

#### A Quick Aside About Security

Although I do not have an exact mechanism, it is conceivable that one could compromise a computer using a maliciously constructed rom. One could theoretically write to or read from the 60kb of memory after the end of the memory array allocated for the interpreter. You would be able to do this by taking advantage of the 0xFX1E instruction, add the values of I and V[X] and store the result in I. One execution of this instruction would allow you to access up to address &memory + 0x10FE and you can write or read up to 16 contiguous bytes using either 0xFX55 or 0xFX65 respectively, these instructions either store registers V[0] through V[X] in memory starting at I or they read bytes from memory starting at I and store them in V[0] through V[X]. I do not have a clear idea of how this would be used exactly, but it would be something along the lines of finding a function in memory that can be called with an op code that is within 60kb of the end of the memory array and injecting malicious instructions encoded as bytes into it that can then be executed when the function is called. A simple way to avoid this would be along the lines of:

    do_op(op);
    if(I > 4080) I = 4080;

which ensures that the above exploit cannot access out of range of memory by the method I described alternatively, you could add a similar control on instructions that change the value of I. It does introduce a problem, which is it actually constrains the available space for writing the rom, typically the end of the rom contains sprite data (although it doesn't have to), which would mean that the last sprite stored can be at memory[4080] and it can only be up to 15 bytes. A regular code block of instructions, however, could still go up to the end of memory, with the last instruction starting at memory[4094]. I have not implemented the above fix as I plan to demonstrate, with slightly modified source code for illustration purposes, the exploit.\
#### Implementing the Exploit
I'm going to wrecklessly write to the bytes immediately after the end of the memory array. So to keep track of this, I've added a pointer to the address of the second byte (for some reason the byte after the end did not work correctly, the compliler may append the array with a const char '\0') after the end of the memory array

    char* str = &memory[0x1001];

The type is a small clue, but this is only a small demonstartion. I then wrote a small rom:

    AFFF //I = 0xFFF
    6002 //V[0] = 0x01
    F01E //I += V[0]
    6048 //V[0] = 0x48
    6165 //V[1] = 0x65
    626C //...
    636C
    646F
    652C
    6620
    6748
    6861
    6963
    6A6B
    6B65
    6C72 //...
    6D00 //V[0xD] = 0x00
    FD55 //load V[0] through V[0xD] into memory starting at I
    6000 //V[0] = 0  
    1224 //jump to address 0x224 (loop indefinitely)

and after the window is closed, one more line of code was added:

    printf("Starting at memory[%i] - %s\n", str - &memory, str);

which prints the following string to the terminal:

    Starting at memory[4097] - Hello, Hacker

here we have successfully 

#### Conclusion
I spent way too much time writing this already, so I'll keep it short.\
This was a really interesting project that allowed me to delve into constrained operating conditions and some lower level C. I'm really proud of some of the optimisations that I made, particularly the trie-like function pointer array for do_op(), which I thought was a creative solution to get around a 34 part if else statement. I'm really happy that I was able to get Pong to run on my computer, having produced over 1000 lines of code to get a 246 byte rom to play correctly. It did help me to appreciate just how complex graphics and, to a greter extent, sound (at least on the surface, I suspect there may be an inversion with greater depth) are. This was also a great opportunity for me to write some tests, which I hadn't had a need for in the past, but they were a great help in debugging. I hope you enjoyed this read if you made it this far (see below for more reading).

#### Appendix 1
There is a online compiler explorer tool called Godbolt. It's not just a cool name, the guy who started the website (godbolt.org) is actually called Matt Godbolt, incredible surname.
I decided to investigate the ADDCARRY instruction as it compiles with GCC 14.2.
I produced some test functions:

    #include<stdint.h>
    uint8_t x[] = {255, 255};
    uint8_t z;
    void addcarry1(){
        uint16_t op = 0x0010;
        x[op>>8] += x [op>>4];
        if (x[op>>8] < x[op>>4]) z = 1;
        else z=0;
    }
    void addcarry2(){
        uint16_t op = 0x0010;
        if ((x[op>>8] += x [op>>4]) < x[op>>4]) z = 1;
        else z=0;
    }
although the symbols are different, I believe this is functionally identical to ADDCARRY().\
The above functions produce the following assembly code

    x:
            .ascii  "\377\377"
    z:
            .zero   1
    addcarry1:
            push    rbp
            mov     rbp, rsp
            mov     WORD PTR [rbp-2], 16
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 8
            movzx   eax, ax
            cdqe
            movzx   ecx, BYTE PTR x[rax]
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 4
            movzx   eax, ax
            cdqe
            movzx   edx, BYTE PTR x[rax]
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 8
            movzx   eax, ax
            add     edx, ecx
            cdqe
            mov     BYTE PTR x[rax], dl
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 8
            movzx   eax, ax
            cdqe
            movzx   edx, BYTE PTR x[rax]
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 4
            movzx   eax, ax
            cdqe
            movzx   eax, BYTE PTR x[rax]
            cmp     dl, al
            jnb     .L2
            mov     BYTE PTR z[rip], 1
            jmp     .L4
    .L2:
            mov     BYTE PTR z[rip], 0
    .L4:
            nop
            pop     rbp
            ret
    addcarry2:
            push    rbp
            mov     rbp, rsp
            mov     WORD PTR [rbp-2], 16
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 8
            movzx   eax, ax
            cdqe
            movzx   ecx, BYTE PTR x[rax]
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 4
            movzx   eax, ax
            cdqe
            movzx   edx, BYTE PTR x[rax]
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 8
            movzx   eax, ax
            add     ecx, edx
            movsx   rdx, eax
            mov     BYTE PTR x[rdx], cl
            cdqe
            movzx   edx, BYTE PTR x[rax]
            movzx   eax, WORD PTR [rbp-2]
            shr     ax, 4
            movzx   eax, ax
            cdqe
            movzx   eax, BYTE PTR x[rax]
            cmp     dl, al
            jnb     .L6
            mov     BYTE PTR z[rip], 1
            jmp     .L8
    .L6:
            mov     BYTE PTR z[rip], 0
    .L8:
            nop
            pop     rbp
            ret

There are a few things to note. Firstly assembly is quite difficult to read compared to C. Secondly, I find it strange that it stores the values of x[0] and x[1] with the instruction .ascii "\377\377", with 377 being the octal representation of 255.\
Thirdly and most importantly, addcarry2 actually results in 3 fewer assembly instructions than addcarry1 (30 vs 33), 1 less shr instruction, which is the bitshift right instruction, and 2 fewer movzx instructions, which copies a value from one register to another, otherwise they are the same. To me, this indicates that addcarry2 is actually very more efficient, not that it really matters for a Chip-8 interpreter running on modern processors that likely have clock frequencies on the order of 10^5 or 10^6 times higher.