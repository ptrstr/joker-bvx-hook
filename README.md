# joker-bvx-hook
A hook for [Jonathan Levin's Joker tool](http://newosxbook.com/tools/joker.html) to support BVX/lzfse compression

## Notice
This has only been tested on Linux so you might have to do changes to build on \*OS (or others)

## Dependencies
This depends on the `dl` library (which is pretty mcuh default on Unix) and [`lzfse`](https://github.com/lzfse/lzfse)

Lots of love to that project as they do all the heavywork <3

## Building
```
$ cd ./joker-bvx-hook
$ gcc -all -shared -fPIC ./joker_hook.c -o joker_hook.so -ldl -llzfse
```

## Using
To use this patch simply set your `LD_PRELOAD` environment variable to the path of the built .so and run Joker

```
$ export LD_PRELOAD=./joker_hook.so
$ joker kernelcache.bvx_compressed
$ unset LD_PRELOAD # Very important or else it'll load into other processes and may cause issues
```
or
```
$ LD_PRELOAD=./joker_hook.so joker kernelcache.bvx_compressed # This'll only apply the preload to Joker
```
