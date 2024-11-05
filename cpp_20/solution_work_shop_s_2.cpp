#include <iostream>
#include <coroutine>
#include <string>
#include <thread>
#include <chrono>

// Awaitable pour simuler un délai asynchrone
struct Awaitable {
    int delay;
    Awaitable(int d) : delay(d) {}

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) const {
        std::thread([handle, delay = this->delay]() {
            std::this_thread::sleep_for(std::chrono::seconds(delay));
            //handle.resume();
        }).detach();
    }
    void await_resume() const noexcept {}
};

// Generator pour récupérer des données en utilisant `co_yield`
struct DataGenerator {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::string current_value;
        auto get_return_object() { return DataGenerator{handle_type::from_promise(*this)}; }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        auto yield_value(std::string value) {
            current_value = std::move(value);
            return std::suspend_always{};
        }
        void return_void() {}
        void unhandled_exception() { std::exit(1); }
    };

    handle_type coro;

    DataGenerator(handle_type h) : coro(h) {}
    ~DataGenerator() { if (coro) coro.destroy(); }

    // Fonction next pour obtenir le prochain morceau de données
    bool next() {
        if (coro) {
            coro.resume();
            return !coro.done();
        }
        return false;
    }

    std::string value() const {
        return coro.promise().current_value;
    }
};

// Fonction `fetch_data_chunks` pour renvoyer les données en morceaux progressifs
DataGenerator fetch_data_chunks(const std::string& source) {
    for (int i = 1; i <= 3; ++i) {
        co_await Awaitable{1};
        co_yield "Chunk " + std::to_string(i) + " from " + source;
    }
}

int main() {
    auto generator = fetch_data_chunks("Source1");
    while (generator.next()) {
        std::string data = generator.value();
        std::cout << data << std::endl;
    }
    return 0;
}