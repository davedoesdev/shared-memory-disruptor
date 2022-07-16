## Streaming example.

This example shows how to stream arbitrary data to a producer through a pipe.

First start the consumer by running on a terminal window:

    node consumer.js

This will initialize the memory.

On a new terminal, pipe any data to the producer. For example,


    { while true; do echo $RANDOM; sleep 0.1; done; } | node producer.js


### Multiple consumers

We'll now run two consumers.

On one terminal start the first consumer:

    ID=0 TOTAL=2 node consumer.js

Start another consumer on another terminal:

    ID=1 TOTAL=2 node consumer.js

Start producing a stream of random numbers:

    { while true; do echo $RANDOM; sleep 0.1; done; } | node producer.js


Now lets try with 100 consumers. This time we'll run the consumers in the
same terminal window as background processes:

    TOTAL=100
    for ID in 0 $(seq $((TOTAL - 1))); do
       ID=$ID TOTAL=$TOTAL node consumer.js | sed "s/^/Consumer $ID: /" &
    done
    { while true; do echo $RANDOM; sleep 0.1; done; } \
      | TOTAL=$TOTAL node producer.js || true

Press control+c to stop the producer and kill the consumers by calling:

    kill $(jobs -p | grep -E -o "[0-9]+ running.*consumer.js" | cut -d' ' -f1)

