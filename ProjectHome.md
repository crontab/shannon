<p align='right'><i>Character develops itself in the stream of life.</i><br>
-- Johann Wolfgang von Goethe<br>
<br>
<b>Currently in development, not finished yet.</b>

<b>Shannon</b> is a new general purpose stream-oriented programming language; it is concise and yet feature rich. Some (and only some) of the features that make Shannon distinct are:<br>
<br>
<ul><li>Streams, FIFOs and UNIX shell-style pipes are first-class concepts in the language. You can connect functions and FIFOs within your program similar to the way you connect processes with pipes in the UNIX shell. These constructs in Shannon, however, allow any structured data to be passed through the pipes, not only characters. These features allow for more concise and readable code for chained data processing and inter-thread communication.</li></ul>

<ul><li><i>State</i> is a special type of function that returns a reference to its own internal (local) data and nested functions. In effect, states implement classes in terms of OOP.</li></ul>

<ul><li><i>(Planned for future releases)</i> Special type of modules marked as "persistent" is an effective replacement for databases and SQL - you can access persistent shared data using native Shannon constructs, eliminating the need for an extra query language.</li></ul>

<ul><li>Intuitive and minimalist syntax and semantics. Particularly "minimalist semantics" means less things to remember - more possibilities.</li></ul>

<ul><li>Shannon is statically-typed although it provides dynamic typing facilities as well.</li></ul>

The current implementation is a monolithic compiler+VM binary. It is written in fairly straightforward C++ which produces a very compact binary.<br>
<br>
<pre><code>'Run, Rabbit, Run!' || preplace('Rabbit', 'Shannon') || sio<br>
<br>
def dow = (Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday)<br>
def shortDowNames = {Monday = 'Mon', Tuesday = 'Tue', Wednesday = 'Wed',<br>
    Thursday = 'Tue', Friday = 'Fri', Saturday = 'Sat', Sunday = 'Sun'}<br>
<br>
for dow, name = shortDowNames<br>
{<br>
    sio &lt;&lt; name<br>
    if dow &gt;= Saturday:<br>
        sio &lt;&lt; ' (Yay! Weekend! Still working though.)'<br>
    sio &lt;&lt; endl<br>
}<br>
</code></pre>