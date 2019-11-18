# Gothello Daemon C Client Library
Bart Massey

The C client "library" for the Gothello daemon
[`gthd`](http://github.com/pdx-cs-ai/gothello-gthd) is a standard
C module including a `.c` and a `.h` file.  You will want to
include `gthclient.h` in your C or C++ program, and build
and link against `gthclient.o`.

If you're using C++, don't forget that you need to do
choose exactly one of the following two options:

1. Compile `gthclient.c` with the C++ compiler
   and include `gthclient.h`

2. Compile `gthclient.c` with the C compiler, then include
   `gthclient.h` using the extern syntax, i.e.

   ```C++
   extern "C" {
   #include "gthclient.h"
   }
   ```

Failure to do one of these (or doing both) will lead to
confusing error messages.

In brief, the client stubs are used by calling
`gth_start_game()` which automatically connects to the
specified server on the specified host as the specified
player.  (See the documentation on running a Gothello game
for details.)  The code then handles the details of making
moves and getting moves from the server, using the
`gth_make_move()` and `gth_get_move()` functions.  These accept
and return move strings: be careful, as there are a couple
of bits of funny business here.  First, a pass is
represented as `".p"` on input and output.  Second, the string
returned by `gth_get_move()` is simply written into the given
pointer: it needs to be pointing at 3 or more writable bytes
(2 for the string plus 1 for the null terminator) or you are
likely to trash memory.

Time control tracking is performed by the client,
which caches a bunch of state information about the game
in progress in global variables.

There is a [manual page](http://htmlpreview.github.io/?https://github.com/pdx-cs-ai/gothello-libclient-c/man/libgthgame.3.html) available in
a variety of formats in the [`man`](man/) subdirectory of this
directory.  Note that this client library is essentially
untested, and I currently have no client with which to
exercise it.  There are likely to be bugs for a bit, until I
have remedied this situation.

Let me know if there are questions or bugs.
