# Running the BF Interpreter
Just run the following commands to build this thing:

```shell
$ make

$ ./compiler.out <insert bf file here>
```

## Timing (Mandelbrot)
- Before anything: 44.63s
- After pre-computing loop jumps: 24.38s
- After indirect gotos / threaded interpreter: 18.15s

### With Profiling
- Just with counting instructions: 17.63s
- With loop profiling: 35.85s