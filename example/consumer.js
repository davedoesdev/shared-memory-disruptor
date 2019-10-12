const { Disruptor } = require('..');
const d = new Disruptor('/example', 1000, 4, 1, 0, false, true); // <1>
async function test() {
    let sum = 0, i = 0;
    while (i < 1000000) {
        const { bufs } = await d.consumeNew(); // <2>
        for (let buf of bufs) {
            for (let j = 0; j < buf.length; j += 4) {
                sum += buf.readUInt32LE(j, true);
                i += 1;
            }
        }
        d.consumeCommit(); // <3>
    }
    console.log(sum);
}
test();
