#include <common.hpp>

static void print_menu();
static int register_process(int server_fd);
static int log_in_process(int server_fd);
static int get_type_authorized(int server_fd);
static int send_disconnect(int server_fd);
static void* server_listen_process(void *arg);

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: ./client ip_addr port\n");
        exit(EXIT_FAILURE);
    }

    int client_fd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    pthread_t listen_thread_id;
    pthread_attr_t thread_attr;
    int status = 0;

    if (pthread_attr_init(&thread_attr) == -1)
    {
        handle_error("pthread_attr_init(): ");
    }

    if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED) == 1)
    {
        handle_error("pthread_attr_setdetachstate(): ");
    }

    server_addr.sin_family = AF_INET,
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY),
    server_addr.sin_port = htons(atoi(argv[2])),

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1)
    {
        handle_error("socket(): ");
    }

    if (connect(client_fd, (struct sockaddr *)&server_addr, server_len) == -1)
    {
        handle_error("connect(): ");
    }

    if (get_type_authorized(client_fd) == -1)
    {
        fprintf(stderr, "Authorized on server failed!\n");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "Authorized is success!" << std::endl;

    status = pthread_create(&listen_thread_id, &thread_attr, &server_listen_process, static_cast<void*>(&client_fd));
    if (status == -1)
    {
        handle_error("listen thread create failed");
    }

    std::cout << "Listen thread create success!" << std::endl;

    do 
    {
        std::string msg_buf;
        std::getline(std::cin, msg_buf, '\n');
        if (msg_buf.empty())
        {
            continue;
        }

        if (msg_buf == "/disconnect")
        {
            status = send_disconnect(client_fd);
            if (status == -1)
            {
                std::cout << "Disconect send failed!" << std::endl;
            }
            else
            {
                std::cout << "Disconect success send!" << std::endl;
            }
            break;
        }

        nlohmann::json request_object;
        std::string res_str;
        request_object[COMMAND_KEY] = MESSAGE;
        request_object[MESSAGE_KEY] = msg_buf;
        res_str = request_object.dump();

        std::string req_msg(request_object.dump());
        status = send(client_fd, static_cast<const void*>(res_str.c_str()), res_str.size(), 0);

    } while (status != -1);

    close(client_fd);

    exit(EXIT_SUCCESS);
}

static void print_menu()
{
    printf("Allow commands:\n"
           "1 --> Register\n"
           "2 --> Log in\n"
    );
}

int register_process(int server_fd)
{
    printf("register_process\n");
    nlohmann::json json_object;
    nlohmann::json response_json_object;
    std::string username, password, res;
    int status = 0;
    char msg_buf[MSG_SIZE];

    memset(msg_buf, 0, MSG_SIZE);

    std::cout << "Enter username: " << std::endl;
    std::cin >> username;

    std::cout << "Enter password: " << std::endl;
    std::cin >> password;

    json_object[COMMAND_KEY] = REGISTER;
    json_object[USERNAME_KEY] = username;
    json_object[PASSWORD_KEY] = password;

    res = json_object.dump();

    status = send(server_fd, (void*)res.c_str(), res.size(), 0);

    if (recv(server_fd, msg_buf, MSG_SIZE, 0) == 0)
    {
        status = -1;
    }
    else
    {
        response_json_object = nlohmann::json::parse(msg_buf);
        if (response_json_object.contains(STATUS_KEY) && response_json_object.contains(REASON_KEY))
        {
            if (response_json_object[STATUS_KEY] == 200)
            {
                std::cout << "Register success!" << std::endl;
                status = 0;
            }
            else
            {
                status = -1;
                std::cerr << "Cannot register client. Reason: " << response_json_object[REASON_KEY] << std::endl;
            }
        }
    }

    return status;
}

int log_in_process(int server_fd)
{
    printf("log_in_process\n");
    nlohmann::json response_json_object;
    nlohmann::json json_object;
    std::string username, password, res;
    int status = 0;
    char msg_buf[MSG_SIZE];

    memset(msg_buf, 0, MSG_SIZE);

    std::cout << "Enter username: " << std::endl;
    std::cin >> username;

    std::cout << "Enter password: " << std::endl;
    std::cin >> password;

    json_object[COMMAND_KEY] = LOG_IN;
    json_object[USERNAME_KEY] = username;
    json_object[PASSWORD_KEY] = password;

    res = json_object.dump();

    status = send(server_fd, (void*)res.c_str(), res.size(), 0);

    if (recv(server_fd, msg_buf, MSG_SIZE, 0) == 0)
    {
        status = -1;
    }
    else
    {
        response_json_object = nlohmann::json::parse(msg_buf);
        if (response_json_object.contains(STATUS_KEY) && response_json_object.contains(REASON_KEY))
        {
            if (response_json_object[STATUS_KEY] == 200)
            {
                std::cout << "Log in success!" << std::endl;
                status = 0;
            }
            else
            {
                status = -1;
                std::cerr << "Cannot log in client. Reason: " << response_json_object[REASON_KEY] << std::endl;
            }
        }
    }

    return status;
}

int get_type_authorized(int server_fd)
{
    int is_valid_type = 1;
    int status = 0;
    uint8_t auth_type;

    while (is_valid_type)
    {
        print_menu();

        std::cin >> auth_type;
        switch (auth_type)
        {
            case '1': // register
                status = register_process(server_fd);
                if (status == 0)
                {
                    is_valid_type = 0;
                } 
                break;

            case '2': // log in
                status = log_in_process(server_fd);
                if (status == 0)
                {
                    is_valid_type = 0;
                }
                break;

            default:
                printf("Unknown command!\n");
                break;

        }

        sync();
        fflush(stdin);
    }

    return status;
}

int send_disconnect(int server_fd)
{
    nlohmann::json request_object;

    request_object[COMMAND_KEY] = DISCONNECT;
    request_object[STATUS_KEY] = 404;
    request_object[REASON_KEY] = "Client left";

    std::string req_msg(request_object.dump());

    return send(server_fd, static_cast<const void *>(req_msg.c_str()), req_msg.size(), 0);
}

void* server_listen_process(void *arg)
{
    int sock_server_fd = *((int *)arg);
    char msg_buf[MSG_SIZE];


    memset(msg_buf, 0, MSG_SIZE);
    while (recv(sock_server_fd, static_cast<void *>(msg_buf), MSG_SIZE, 0) > 0)
    {
        nlohmann::json msg_object = nlohmann::json::parse(msg_buf);

        if (msg_object.contains(COMMAND_KEY) 
            && msg_object[COMMAND_KEY] == MESSAGE 
            && msg_object.contains(USERNAME_KEY)
            && msg_object.contains(MESSAGE_KEY))
        {
            std::cout << msg_object[USERNAME_KEY] << ": " << msg_object[MESSAGE_KEY] << std::endl;
        }
        else if (msg_object.contains(STATUS_KEY))
        {
            if (msg_object[STATUS_KEY] == 200)
            {
                std::cout << "Msg success send!" << std::endl;
            }
            else
            {
                std::cout << "Message send has Failed!" << std::endl;
            }
        } 


        memset(msg_buf, 0, MSG_SIZE);
    }

    return NULL;
}