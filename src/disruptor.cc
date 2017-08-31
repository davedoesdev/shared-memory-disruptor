// Start by just creating shared memory and checking multiple processes can read
// and write to it.
// Then get the atomic operations working (add and cas).
// Then implement the algorithm. Option to clear all the memory?

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <memory>
#include <chrono>
#include <thread>
#include <napi.h>

typedef uint64_t sequence_t;

class Disruptor : public Napi::ObjectWrap<Disruptor>
{
public:
    Disruptor(const Napi::CallbackInfo& info);
    ~Disruptor();

    static void Initialize(Napi::Env env, Napi::Object exports);

    // Unmap the shared memory. Don't access it again from this process!
    void Release(const Napi::CallbackInfo& info);

    // Return unconsumed values for a consumer
    Napi::Value ConsumeNewSync(const Napi::CallbackInfo& info); 
    void ConsumeNewAsync(const Napi::CallbackInfo& info); 

    // Update consumer sequence without consuming more
    void ConsumeCommit(const Napi::CallbackInfo&);

    // Claim a slot for writing a value
    Napi::Value ProduceClaimSync(const Napi::CallbackInfo& info);
    void ProduceClaimAsync(const Napi::CallbackInfo& info);

    // Commit a claimed slot.
    Napi::Value ProduceCommitSync(const Napi::CallbackInfo& info);
    void ProduceCommitAsync(const Napi::CallbackInfo& info);

private:
    friend class ConsumeNewAsyncWorker;
    friend class ProduceClaimAsyncWorker;
    friend class ProduceCommitAsyncWorker;

    int Release();
    void UpdatePending(sequence_t seq_consumer, sequence_t seq_cursor);
    Napi::Value ConsumeNewSync(const Napi::Env& env);
    void ConsumeCommit();
    Napi::Value ProduceClaimSync(const Napi::Env& env);
    Napi::Value ProduceCommitSync(const Napi::Env& env,
                                  const Napi::Object& obj);

    uint32_t num_elements;
    uint32_t element_size;
    uint32_t num_consumers;
    uint32_t consumer;
    int32_t spin_sleep;

    size_t shm_size;
    void* shm_buf;
    
    sequence_t *consumers; // for each consumer, next slot to read
    sequence_t *cursor;    // next slot to be filled
    sequence_t *next;      // next slot to claim
    uint8_t* elements;
    sequence_t *ptr_consumer;

    sequence_t pending_seq_consumer;
    sequence_t pending_seq_cursor;
};

template <uint32_t CB>
class DisruptorAsyncWorker : public Napi::AsyncWorker
{
public:
    DisruptorAsyncWorker(const Napi::CallbackInfo& info) :
        Napi::AsyncWorker(info[CB].As<Napi::Function>()),
        env(info.Env()),
        disruptor_ref(Napi::Persistent(info.This().As<Napi::Object>()))
    {
    }

protected:
    virtual Napi::Value Execute(Disruptor *disruptor) = 0;

    void Execute() override
    {
        Disruptor *disruptor = Napi::ObjectWrap<Disruptor>::Unwrap(
            disruptor_ref.Value());
        result = Napi::Persistent(Execute(disruptor));
    }

    void OnOK() override
    {
        Callback().MakeCallback(
            Receiver().Value(),
            std::initializer_list<napi_value>{env.Null(), result.Value()});
    }

    Napi::Env env;
    Napi::ObjectReference disruptor_ref;
    Napi::Reference<Napi::Value> result;
};

struct CloseFD
{
    void operator()(int *fd)
    {
        close(*fd);
        delete fd;
    }
};

void ThrowErrnoError(const Napi::CallbackInfo& info, const char *msg)
{
    int errnum = errno;
    char buf[1024] = {0};
    auto errmsg = strerror_r(errnum, buf, sizeof(buf));
    static_assert(std::is_same<decltype(errmsg), char*>::value,
                  "strerror_r must return char*");
    throw Napi::Error::New(info.Env(), 
        std::string(msg) + ": " + (errmsg ? errmsg : std::to_string(errnum)));
}

Disruptor::Disruptor(const Napi::CallbackInfo& info) :
    Napi::ObjectWrap<Disruptor>(info),
    shm_buf(MAP_FAILED)
{
    // TODO: How do we ensure non-initers don't read while init is happening?

    // Arguments
    Napi::String shm_name = info[0].As<Napi::String>();
    num_elements = info[1].As<Napi::Number>();
    element_size = info[2].As<Napi::Number>();
    num_consumers = info[3].As<Napi::Number>();
    consumer = info[4].As<Napi::Number>();
    bool init = info[5].As<Napi::Boolean>();

    if (info[6].IsNumber())
    {
        spin_sleep = info[6].As<Napi::Number>();
    }
    else
    {
        spin_sleep = -1;
    }

    // Open shared memory object
    std::unique_ptr<int, CloseFD> shm_fd(new int(
        shm_open(shm_name.Utf8Value().c_str(),
                 O_CREAT | O_RDWR | (init ? O_TRUNC : 0),
                 S_IRUSR | S_IWUSR)));
    if (*shm_fd < 0)
    {
        ThrowErrnoError(info, "Failed to open shared memory object");
    }

    // Allow space for all the elements,
    // a sequence number for each consumer,
    // the cursor sequence number (last filled slot) and
    // the next sequence number (first free slot).
    shm_size = (num_consumers + 2) * sizeof(sequence_t) +
               num_elements * element_size;

    // Resize the shared memory if we're initializing it.
    // Note: ftruncate initializes to null bytes.
    if (init && (ftruncate(*shm_fd, shm_size) < 0))
    {
        ThrowErrnoError(info, "Failed to size shared memory");
    }

    // Map the shared memory
    shm_buf = mmap(NULL,
                   shm_size,
                   PROT_READ | PROT_WRITE, MAP_SHARED,
                   *shm_fd,
                   0);
    if (shm_buf == MAP_FAILED)
    {
        ThrowErrnoError(info, "Failed to map shared memory");
    }

    consumers = static_cast<sequence_t*>(shm_buf);
    cursor = &consumers[num_consumers];
    next = &cursor[1];
    elements = reinterpret_cast<uint8_t*>(&next[1]);
    ptr_consumer = &consumers[consumer];

    pending_seq_consumer = 0;
    pending_seq_cursor = 0;
}

Disruptor::~Disruptor()
{
    Release();
}

int Disruptor::Release()
{
    if (shm_buf != MAP_FAILED)
    {
        int r = munmap(shm_buf, shm_size);

        if (r < 0)
        {
            return r;
        }

        shm_buf = MAP_FAILED;
    }

    return 0;
}

void Disruptor::Release(const Napi::CallbackInfo& info)
{
    if (Release() < 0)
    {
        ThrowErrnoError(info, "Failed to unmap shared memory");
    }
}

Napi::Value Disruptor::ConsumeNewSync(const Napi::Env& env)
{
    // Return all elements [&consumers[consumer], cursor)

    // Commit previous consume
    ConsumeCommit();

    Napi::Array r = Napi::Array::New(env);
    sequence_t seq_consumer, pos_consumer;
    sequence_t seq_cursor, pos_cursor;

    do
    {
        // TODO: Do we need to access consumer sequence atomically if we know
        // only this thread is updating it?
        seq_consumer = __sync_val_compare_and_swap(ptr_consumer, 0, 0);
        seq_cursor = __sync_val_compare_and_swap(cursor, 0, 0);
        pos_consumer = seq_consumer % num_elements;
        pos_cursor = seq_cursor % num_elements;

        if (pos_cursor > pos_consumer)
        {
            r.Set(0U, Napi::Buffer<uint8_t>::New(
                env,
                elements + pos_consumer * element_size,
                (pos_cursor - pos_consumer) * element_size));

            UpdatePending(seq_consumer, seq_cursor);
            break;
        }

        if (seq_cursor != seq_consumer)
        {
            r.Set(0U, Napi::Buffer<uint8_t>::New(
                env,
                elements + pos_consumer * element_size,
                (num_elements - pos_consumer) * element_size));

            if (pos_cursor > 0)
            {
                r.Set(1U, Napi::Buffer<uint8_t>::New(
                    env,
                    elements,
                    pos_cursor * element_size));
            }

            UpdatePending(seq_consumer, seq_cursor);
            break;
        }

        if (spin_sleep < 0)
        {
            break;
        }

        if (spin_sleep > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(spin_sleep));
        }
    }
    while (true);

    return r;
}

Napi::Value Disruptor::ConsumeNewSync(const Napi::CallbackInfo& info)
{
    return ConsumeNewSync(info.Env());
}

class ConsumeNewAsyncWorker : public DisruptorAsyncWorker<0>
{
public:
    ConsumeNewAsyncWorker(const Napi::CallbackInfo& info) :
        DisruptorAsyncWorker<0>(info)
    {
    }

protected:
    Napi::Value Execute(Disruptor* disruptor) override
    {
        return disruptor->ConsumeNewSync(env);
    }
};

void Disruptor::ConsumeNewAsync(const Napi::CallbackInfo& info)
{
    ConsumeNewAsyncWorker *worker = new ConsumeNewAsyncWorker(info);
    worker->Queue();
}

void Disruptor::ConsumeCommit(const Napi::CallbackInfo&)
{
    ConsumeCommit();
}

void Disruptor::UpdatePending(sequence_t seq_consumer, sequence_t seq_cursor)
{
    pending_seq_consumer = seq_consumer;
    pending_seq_cursor = seq_cursor;
}

void Disruptor::ConsumeCommit()
{
    if (pending_seq_cursor)
    {
        __sync_val_compare_and_swap(ptr_consumer,
                                    pending_seq_consumer,
                                    pending_seq_cursor);
        pending_seq_consumer = 0;
        pending_seq_cursor = 0;
    }
}

Napi::Value Disruptor::ProduceClaimSync(const Napi::Env& env)
{
    sequence_t seq_next, pos_next;
    sequence_t seq_consumer, pos_consumer;
    bool can_claim;

    do
    {
        seq_next = __sync_val_compare_and_swap(next, 0, 0);
        pos_next = seq_next % num_elements;

        can_claim = true;

        for (uint32_t i = 0; i < num_consumers; ++i)
        {
            seq_consumer = __sync_val_compare_and_swap(&consumers[i], 0, 0);
            pos_consumer = seq_consumer % num_elements;

            if ((pos_consumer == pos_next) && (seq_consumer != seq_next))
            {
                can_claim = false;
                break;
            }
        }

        if (can_claim &&
            (__sync_val_compare_and_swap(next, seq_next, seq_next + 1) ==
             seq_next))
        {
            Napi::Buffer<uint8_t> r = Napi::Buffer<uint8_t>::New(
                env,
                elements + pos_next * element_size,
                element_size);

            r.Set("seq_next", Napi::Number::New(env, seq_next));

            return r;
        }

        if (spin_sleep < 0)
        {
            return Napi::Buffer<uint8_t>::New(env, 0);
        }

        if (spin_sleep > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(spin_sleep));
        }
    }
    while (true);
}

Napi::Value Disruptor::ProduceClaimSync(const Napi::CallbackInfo& info)
{
    return ProduceClaimSync(info.Env());
}

class ProduceClaimAsyncWorker : public DisruptorAsyncWorker<0>
{
public:
    ProduceClaimAsyncWorker(const Napi::CallbackInfo& info) :
        DisruptorAsyncWorker<0>(info)
    {
    }

protected:
    Napi::Value Execute(Disruptor* disruptor) override
    {
        return disruptor->ProduceClaimSync(env);
    }
};

void Disruptor::ProduceClaimAsync(const Napi::CallbackInfo& info)
{
    ProduceClaimAsyncWorker *worker = new ProduceClaimAsyncWorker(info);
    worker->Queue();
}

Napi::Value Disruptor::ProduceCommitSync(const Napi::Env& env,
                                         const Napi::Object& obj)
{
    sequence_t seq_next = obj.Get("seq_next").As<Napi::Number>().Int64Value();
    
    do
    {
        if (__sync_val_compare_and_swap(cursor, seq_next, seq_next + 1) ==
            seq_next)
        {
            return Napi::Boolean::New(env, true);
        }

        if (spin_sleep < 0)
        {
            return Napi::Boolean::New(env, false);
        }

        if (spin_sleep > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(spin_sleep));
        }

    }
    while (true);
}

Napi::Value Disruptor::ProduceCommitSync(const Napi::CallbackInfo& info)
{
    return ProduceCommitSync(info.Env(), info[0].As<Napi::Object>());
}

class ProduceCommitAsyncWorker : public DisruptorAsyncWorker<1>
{
public:
    ProduceCommitAsyncWorker(const Napi::CallbackInfo& info) :
        DisruptorAsyncWorker<1>(info),
        obj_ref(Napi::Persistent(info[0].As<Napi::Object>()))
    {
    }

protected:
    Napi::Value Execute(Disruptor* disruptor) override
    {
        return disruptor->ProduceCommitSync(env, obj_ref.Value());
    }

    Napi::ObjectReference obj_ref;
};

void Disruptor::ProduceCommitAsync(const Napi::CallbackInfo& info)
{
    ProduceCommitAsyncWorker *worker = new ProduceCommitAsyncWorker(info);
    worker->Queue();
}

void Disruptor::Initialize(Napi::Env env, Napi::Object exports)
{
    exports.Set("Disruptor", DefineClass(env, "Disruptor",
    {
        InstanceMethod("release", &Disruptor::Release),
        InstanceMethod("consumeNew", &Disruptor::ConsumeNewAsync),
        InstanceMethod("consumeNewSync", &Disruptor::ConsumeNewSync),
        InstanceMethod("consumeCommit", &Disruptor::ConsumeCommit),
        InstanceMethod("produceClaim", &Disruptor::ProduceClaimAsync),
        InstanceMethod("produceClaimSync", &Disruptor::ProduceClaimSync),
        InstanceMethod("produceCommit", &Disruptor::ProduceCommitAsync),
        InstanceMethod("produceCommitSync", &Disruptor::ProduceCommitSync),
    }));
}

void Initialize(Napi::Env env, Napi::Object exports, Napi::Object module)
{
    Disruptor::Initialize(env, exports);
}

NODE_API_MODULE(disruptor, Initialize)
