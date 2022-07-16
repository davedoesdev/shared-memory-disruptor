This is an implementation of the [LMAX
Disruptor](https://lmax-exchange.github.io/disruptor/) for Node.js.

The LMAX Disruptor is a lock-free data structure and algorithm for fast
message passing. The [reference
implementation](https://github.com/LMAX-Exchange/disruptor) is written
in Java and supports passing messages between threads.

The implementation here supports passing messages between Node.js
processes or worker threads. It’s written in C++ as a Node Addon which
is callable from Javascript.

A set of tests is provided for single and multiple processes and
multiple workers.

API documentation is available
[here](http://rawgit.davedoesdev.com/davedoesdev/shared-memory-disruptor/master/docs/index.html).
Promises (`async/await`), callback style and synchronous methods are
supported.

Currently only POSIX shared memory is supported.

# Example

Here’s a program which writes a million random numbers to a Disruptor.
Run this first in one terminal.

**producer.js.**

``` javascript
const { Disruptor } = require('shared-memory-disruptor');
const d = new Disruptor('/example', 1000, 4, 1, -1, true, true); // 
async function test() {
    let sum = 0;
    for (let i = 0; i < 1000000; i += 1) {
        const n = Math.floor(Math.random() * 100);
        const { buf } = await d.produceClaim(); // 
        buf.writeUInt32LE(n, 0, true);
        await d.produceCommit(); // 
        sum += n;
    }
    console.log(sum);
}
test();
```

  - Use a Disruptor on the shared memory object `/example`. The
    Disruptor has 1000 elements of 4 bytes each. It has 1 consumer, and
    because we’re only going to produce data, we give an invalid a
    consumer index (-1) which won’t be used. We’ll initialize the
    Disruptor and spin when the Disruptor is full.

  - Grab an element in the Disruptor to fill in.

  - Tell the Disruptor we’ve filled in the elements.

Here’s another program which reads a million random numbers from the
same Disruptor. Run this in a second terminal.

**consumer.js.**

``` javascript
const { Disruptor } = require('shared-memory-disruptor');
const d = new Disruptor('/example', 1000, 4, 1, 0, false, true); // 
async function test() {
    let sum = 0, i = 0;
    while (i < 1000000) {
        const { bufs } = await d.consumeNew(); // 
        for (let buf of bufs) {
            for (let j = 0; j < buf.length; j += 4) {
                sum += buf.readUInt32LE(j, true);
                i += 1;
            }
        }
        d.consumeCommit(); // 
    }
    console.log(sum);
}
test();
```

  - Use the Disruptor that [formalpara\_title](#producer) initialized on
    the shared memory object `/example`. We must specify the same number
    of elements (1000) of the same size (4 bytes), and the same number
    of consumers (1). We’ll be the only consumer (index 0), won’t
    initialize the Disruptor and will spin when the Disruptor is empty.

  - Read new data from the Disruptor. We get an array of
    [Buffer](https://nodejs.org/dist/latest-v8.x/docs/api/buffer.html)s,
    each Buffer containing a whole number of elements.

  - Tell the Disruptor we’ve finished processing the data.

Both programs print the same result.

# Streaming example

You can produce and consume arbitrary data through streams.

**consumer.js.**

``` javascript
// You must run the consumer before the producer
const { Disruptor, DisruptorReadStream } = require('shared-memory-disruptor');

const d = new Disruptor('/stream', 1000, 1, 1, 0, true, false);
const rs = new DisruptorReadStream(d)

rs.pipe(process.stdout);
```

**producer.js.**

``` javascript
const { Disruptor, DisruptorWriteStream } = require('shared-memory-disruptor');

process.stdin.pipe(
    new DisruptorWriteStream(
        new Disruptor('/stream', 1000, 1, 1, 0, false, false)
    )
);
```

First start the consumer by running on a terminal window:

    node consumer.js

This will initialize the memory.

On a new terminal, pipe any data to the producer. For example,

    { while true; do echo $RANDOM; sleep 0.1; done; } | node producer.js

# Install

``` bash
npm install shared-memory-disruptor
```

# Licence

[MIT](LICENCE)

# Test

``` bash
grunt test
```

# Coverage

``` bash
grunt coverage
```

LCOV results are available
[here](http://rawgit.davedoesdev.com/davedoesdev/shared-memory-disruptor/master/coverage/lcov-report/index.html).

Coveralls page is
[here](https://coveralls.io/r/davedoesdev/shared-memory-disruptor).
