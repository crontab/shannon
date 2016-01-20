THIS IS A WORK IN PROGRESS
----------------

**Shannon** is a new general purpose stream-oriented programming language; it is concise and yet feature rich. Some (and only some) of the features that make Shannon distinct are:

* Streams, FIFOs and UNIX shell-style pipes are first-class concepts in the language. You can connect functions and FIFOs within your program similar to the way you connect processes with pipes in the UNIX shell. These constructs in Shannon, however, allow any structured data to be passed through the pipes, not only characters. These features allow for more concise and readable code for chained data processing and inter-thread communication.

* State is a special type of function that returns a reference to its own internal (local) data and nested functions. In effect, states implement classes in terms of OOP.

* (Planned for future releases) Special type of modules marked as "persistent" is an effective replacement for databases and SQL - you can access persistent shared data using native Shannon constructs, eliminating the need for an extra query language.

* Intuitive and minimalist syntax and semantics. Particularly "minimalist semantics" means less things to remember - more possibilities.

* Shannon is statically-typed although it provides dynamic typing facilities as well.
The current implementation is a monolithic compiler+VM binary. It is written in fairly straightforward C++ which produces a very compact binary.

```
'Run, Rabbit, Run!' || preplace('Rabbit', 'Shannon') || sio

def dow = (Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday)
def shortDowNames = {Monday = 'Mon', Tuesday = 'Tue', Wednesday = 'Wed',
    Thursday = 'Tue', Friday = 'Fri', Saturday = 'Sat', Sunday = 'Sun'}

for dow, name = shortDowNames
{
    sio << name
    if dow >= Saturday:
        sio << ' (Yay! Weekend! Still working though.)'
    sio << endl
}
```
