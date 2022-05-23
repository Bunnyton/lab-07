#include <http_server.hpp>
#include <suggest.hpp>

void suggest_updater(Suggest &sug, const std::shared_ptr<std::timed_mutex>& mutex){
  std::this_thread::sleep_for(std::chrono::minutes(15));
  mutex->lock();
  sug.update();
  mutex->unlock();
}

int main(int argc, char* argv[])
{
    try
    {
//         Check command line arguments.
        if (argc != 3)
        {
            std::cerr <<
                "Usage: http-server-sync <address> <port>\n" <<
                "Example:\n" <<
                "    http-server-sync 0.0.0.0 80\n";
            return EXIT_FAILURE;
        }
        auto const address = net::ip::make_address(argv[1]);
        auto const port = static_cast<unsigned short>(std::atoi(argv[2]));

//        auto const address = net::ip::make_address("127.0.0.1");
//        auto const port = static_cast<unsigned short>(std::atoi("80"));

        // The io_context is required for all I/O
        net::io_context ioc{1};

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};

        Suggest sug("../suggestions.json");
          std::shared_ptr<std::timed_mutex> mutex =
            std::make_shared<std::timed_mutex>();

        std::thread(suggest_updater, std::ref(sug), std::ref(mutex)).detach();

        for(;;)
        {
            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            // Launch the session, transferring ownership of the socket
            std::thread{std::bind(
                &do_session,
                std::move(socket),
                sug,
                mutex)}.detach();
        }
    }
    catch (const std::exception& ec)
    {
        std::cerr << "Error: " << ec.what() << std::endl;
        return EXIT_FAILURE;
    }
}