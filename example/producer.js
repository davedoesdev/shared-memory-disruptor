const { Disruptor } = require('..');
const d = new Disruptor('/example', 1000, 4, 1, -1, true, true); // <1>
async function test() {
    let sum = 0;
    for (let i = 0; i < 1000000; i += 1) {
        const n = Math.floor(Math.random() * 100);
        const { buf } = await d.produceClaim(); // <2>
        buf.writeUInt32LE(n, 0, true);
        await d.produceCommit(); // <3>
        sum += n;
    }
    console.log(sum);
}
test();
