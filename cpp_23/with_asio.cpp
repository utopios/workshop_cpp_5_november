#include <iostream>
#include <asio.hpp>
#include <execution>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>
#include <condition_variable>

namespace std {
    namespace execution {

        class static_thread_pool {
        public:
            explicit static_thread_pool(size_t numThreads) {
                start(numThreads);
            }

            ~static_thread_pool() {
                stop();
            }

            template <typename F>
            void execute(F&& f) {
                std::thread(std::forward<F>(f)).detach();  // Détacher le thread pour l'exécution
            }

            // Attendre la fin de toutes les tâches
            void wait() {
                stop();
            }

        private:
            std::vector<std::thread> threads;
            std::queue<std::function<void()>> tasks;
            std::mutex queueMutex;
            std::condition_variable condition;
            bool stopPool = false;

            // Démarrer le pool de threads
            void start(size_t numThreads) {
                for (size_t i = 0; i < numThreads; ++i) {
                    threads.emplace_back([this] {
                        while (true) {
                            std::function<void()> task;
                            {
                                std::unique_lock<std::mutex> lock(this->queueMutex);

                                // Attendre qu'il y ait une tâche ou que le pool soit arrêté
                                this->condition.wait(lock, [this] {
                                    return this->stopPool || !this->tasks.empty();
                                });

                                if (this->stopPool && this->tasks.empty()) {
                                    return;
                                }

                                task = std::move(this->tasks.front());
                                this->tasks.pop();
                            }
                            task(); // Exécuter la tâche
                        }
                    });
                }
            }


            void stop() {
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    stopPool = true;
                }

                condition.notify_all();

                for (std::thread &thread : threads) {
                    if (thread.joinable()) {
                        thread.join();
                    }
                }
            }
        };

    }  // namespace execution
}  // namespace std

using asio::ip::tcp;

// Fonction pour gérer une connexion
void handle_connection(tcp::socket socket) {
    try {
        std::vector<char> data(1024);
        // Lire des données
        std::size_t length = socket.read_some(asio::buffer(data));
        std::cout << "Données reçues: " << std::string(data.data(), length) << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Erreur: " << e.what() << std::endl;
    }
}

int main() {
    try {
        asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 12345));

        // Créer un pool d'exécuteurs
        std::execution::static_thread_pool pool(4); // 4 threads dans le pool

        std::cout << "Serveur démarré..." << std::endl;

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            // Distribuer la gestion de la connexion à un exécuteur
            pool.execute([socket = std::move(socket)]() mutable {
                handle_connection(std::move(socket));
            });
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    try {
        asio::io_context io_context;

        // Résoudre l'adresse et le port du serveur
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", "12345");

        // Créer un socket
        tcp::socket socket(io_context);

        // Se connecter à l'un des endpoints
        asio::connect(socket, endpoints);

        // Message à envoyer au serveur
        std::string message = "Hello from client!";

        // Envoyer le message
        asio::write(socket, asio::buffer(message));

        std::cout << "Message envoyé: " << message << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}