# Running the BF Interpreter
Just run the following commands to build this thing:

```shell
$ make

$ ./interpreter.out <insert bf file here>

$ ./compiler.out <insert bf file here>
```

### Compiler Guide
The compiler outputs to standard out, so to get an executable, this would be the best bet:
```shell
$ ./compiler.out myfile.bf > myasm.s
$ gcc myasm.s -o test.out
$ ./test.out
```

### Interpreter Guide
The interpreter can run on a file, with or without profiling. To enable profiling, pass -p as so:
```shell
$ ./interpreter.out -p myfile.bf
```

## Interpreter Timing
### Timing (Mandelbrot)
- Before anything: 44.63s
- After pre-computing loop jumps: 24.38s
- After indirect gotos / threaded interpreter: 18.15s

#### With Profiling
- Just with counting instructions: 17.63s
- With loop profiling: 35.85s

## Compiler Timing
### Timing (Mandelbrot)
- Before anything: 4.41s
- After simplifying zero loops: 4.38s
- After simplifying simple inner loops: 3.99s