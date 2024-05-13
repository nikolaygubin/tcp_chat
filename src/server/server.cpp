#include <common.hpp>

// defines

#define MSG_FROM_PROCESS_SIZE 512
#define NUM_THREADS 10

// global vars

pthread_t thread_ids[NUM_THREADS];

pthread_mutex_t thread_mutex;

nlohmann::json clients_object;

// static functions

static void free_thread(pthread_t thread_id);
static int find_free_thread(pthread_t *thread_ids);

static void set_all_not_connect();
static int load_clients();
static int save_clients();

static std::string get_username(int client_fd);

static int register_client_process(const std::string &username, const std::string &password, int client_fd);
static int log_in_client_process(const std::string &username, const std::string &password, int client_fd);
static int message_client_process(const std::string &msg, int client_fd);
static int disconnect_client_process(int client_fd);

static int abort_client_process(int client_fd, std::string msg);
static int success_client_process(int client_fd);

static void* process_listen_client(void *arg);

int main()
{
    int sock_server_fd, sock_client_fd;
    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    socklen_t serv_len = sizeof(serv_addr), client_len;

    pthread_attr_t attr;

    int free_thread_idx;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = 0;

    if (pthread_mutex_init(&thread_mutex, NULL) == -1)
    {
        handle_error("pthread_mutex_init(): ");
    }

    memset(thread_ids, 0, sizeof(pthread_t) * NUM_THREADS);

    if (pthread_attr_init(&attr) == -1)
    {
        handle_error("pthread_attr_init(): ");
    }

    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 1)
    {
        handle_error("pthread_attr_setdetachstate(): ");
    }

    sock_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_server_fd == -1)
    {
        handle_error("socket(): ");
    }

    if (bind(sock_server_fd, (struct sockaddr *)&serv_addr, serv_len) == -1)
    {
        handle_error("bind(): ");
    }

    if (listen(sock_server_fd, 5) == -1)
    {
        handle_error("listen(): ");
    }

    if (getsockname(sock_server_fd, (struct sockaddr *)&serv_addr, &serv_len) == -1)
    {
        handle_error("getsockname(): ");
    }
    printf("Server port is %d\nServer ip addr is %s\n\n", ntohs(serv_addr.sin_port), inet_ntoa(serv_addr.sin_addr));

    if (load_clients())
    {
        handle_error("Cannot get clients from json ");
    }

    set_all_not_connect();

    while (1)
    {
        if ((sock_client_fd = accept(sock_server_fd, (struct sockaddr *)&client_addr, &client_len)) == -1)
        {
            handle_error("connect(): ");
        }

        while (1)
        {
            free_thread_idx = find_free_thread(thread_ids);
            if (free_thread_idx == NUM_THREADS - 1)
            {
                sleep(3);
                continue;
            }
            break;
        }

        printf("client addr:\nip = %s\nport = %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_create(&thread_ids[free_thread_idx], &attr, &process_listen_client, &sock_client_fd);
    }

    close(sock_server_fd);

    pthread_mutex_destroy(&thread_mutex);

    exit(EXIT_SUCCESS);
}

static void free_thread(pthread_t thread_id)
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        if (thread_id == thread_ids[i])
        {
            thread_ids[i] = 0;
            break;
        }
    }
}

static int find_free_thread(pthread_t *thread_ids)
{
    int i;
    for (i = 0; i < NUM_THREADS; i++)
    {   
        if (thread_ids[i] == 0)
        {
            break;
        }
    }

    return i;
}

static void set_all_not_connect()
{
    if (clients_object.contains(CLIENTS_KEY))
    {
        auto clients = clients_object[CLIENTS_KEY];

        std::for_each(clients.begin(), clients.end(), [](auto &client) {
            client[IS_CONNECT] = false;
            client[SOCKET_FD] = -1;
        });

        clients_object[CLIENTS_KEY] = clients;
        save_clients();
    }

}

static int load_clients()
{
    std::ifstream clients_file(CLIENTS_PATH);
    int status = 0;

    while (pthread_mutex_trylock(&thread_mutex)) 
    {
        sleep(1);
    }

    if (clients_file.is_open()) 
    {
        try
        {
            clients_object = nlohmann::json::parse(clients_file);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
    }
    else
    {
        status = -1;
        goto _exit;
    }

    clients_file.close();

_exit:
    pthread_mutex_unlock(&thread_mutex);
    return status;
}

static int save_clients()
{
    std::ofstream clients_file(CLIENTS_PATH);
    int status = 0;

    while (pthread_mutex_trylock(&thread_mutex)) 
    {
        sleep(1);
    }

    if (clients_file.is_open()) 
    {
        clients_file.clear();
        clients_file << clients_object.dump();
    }
    else
    {
        status = -1;
        goto _exit;
    }

    clients_file.close();

_exit:
    pthread_mutex_unlock(&thread_mutex);
    return status;
}

static int register_client_process(const std::string &username, const std::string &password, int client_fd)
{
    int status = -1;

    if (clients_object.contains("Clients"))
    {
        std::cout << clients_object["Clients"] << std::endl;
        auto clients_vec = clients_object["Clients"];

        auto is_find = std::find_if(clients_vec.begin(), clients_vec.end(), [username](auto client) {
            if (client[USERNAME_KEY] == username) return true;

            return false;
        });

        if (is_find == clients_vec.end())
        {
            nlohmann::json client_object;
            client_object[USERNAME_KEY] = username;
            client_object[PASSWORD_KEY] = password;
            client_object[IS_CONNECT] = true;
            client_object[SOCKET_FD] = client_fd;
            clients_object[CLIENTS_KEY].push_back(client_object);
            status = save_clients();
        }
        else
        {
            status = -1;
        }
    }
    else
    {
        nlohmann::json client_object;
        client_object[USERNAME_KEY] = username;
        client_object[PASSWORD_KEY] = password;
        client_object[IS_CONNECT] = true;
        client_object[SOCKET_FD] = client_fd;
        clients_object[CLIENTS_KEY].push_back(client_object);
        status = save_clients();
    }

    return status;
}

static int log_in_client_process(const std::string &username, const std::string &password, int client_fd)
{
    int status = -1;

    if (clients_object.contains(CLIENTS_KEY))
    {
        auto clients_vec = clients_object[CLIENTS_KEY];

        auto is_find = std::find_if(clients_vec.begin(), clients_vec.end(), [&username, &password, &status, &client_fd](auto &client) {
            if (client[USERNAME_KEY] == username && client[PASSWORD_KEY] == password)
            {
                if (client[IS_CONNECT] == true)
                {
                    status = -2;
                }
                else
                {
                    client[IS_CONNECT] = true;
                    client[SOCKET_FD] = client_fd;
                    status = 0;
                }
                return true;
            } 

            return false;
        });
        
        if (is_find != clients_vec.end() && status == 0)
        {
            clients_object[CLIENTS_KEY] = clients_vec;
            status = save_clients();
        }
    }

    return status;
}

static std::string get_username(int client_fd)
{
    std::string username;

    if (clients_object.contains(CLIENTS_KEY))
    {
        auto clients_vec = clients_object[CLIENTS_KEY];

        auto is_find = std::find_if(clients_vec.begin(), clients_vec.end(), [&client_fd](auto client) {
            if (client[SOCKET_FD] == client_fd && client[SOCKET_FD] != -1) return true;

            return false;
        });

        if (is_find != clients_vec.end())
        {
            username = (*is_find)[USERNAME_KEY];
        }
    }

    return username;
}

static int message_client_process(const std::string &msg, int client_fd)
{
    int status = 0;

    if (clients_object.contains(CLIENTS_KEY))
    {
        auto clients_vec = clients_object[CLIENTS_KEY];

        std::for_each(clients_vec.begin(), clients_vec.end(), [&client_fd, &msg, &status](auto &client) {
            if (client.contains(SOCKET_FD) 
                && client[SOCKET_FD] != client_fd
                && client.contains(IS_CONNECT) 
                && client[IS_CONNECT])
            {
                nlohmann::json msg_object;
                std::string res_msg;
                std::string username = get_username(client_fd);
                if (username.empty())
                {
                    status = -1;
                }

                msg_object[COMMAND_KEY] = MESSAGE;
                msg_object[MESSAGE_KEY] = msg;  
                msg_object[USERNAME_KEY] = username;
                res_msg = msg_object.dump();

                if (send(client[SOCKET_FD], static_cast<const void*>(res_msg.c_str()), res_msg.size(), 0) == -1)
                {
                    std::cerr << "Client " << client_fd << ": send msg to " << client[SOCKET_FD] << " has failed" << std::endl;
                    client[IS_CONNECT] = false;
                    status = save_clients();
                }
            }
        });
    }

    return status;
}

static int disconnect_client_process(int client_fd)
{
    int status = 0;

    if (clients_object.contains(CLIENTS_KEY))
    {
        auto clients_vec = clients_object[CLIENTS_KEY];

        auto is_find = std::find_if(clients_vec.begin(), clients_vec.end(), [&client_fd](auto &client) {
            if (client[SOCKET_FD] == client_fd)
            {
                client[IS_CONNECT] = false;
                client[SOCKET_FD] = -1;
                return true;
            } 

            return false;
        });
        
        if (is_find != clients_vec.end() && status == 0)
        {
            clients_object[CLIENTS_KEY] = clients_vec;
            status = save_clients();
        }
    }

    return status;
}

static int abort_client_process(int client_fd, std::string msg)
{
    nlohmann::json response_object;
    std::string res;
    int status = 0;

    response_object[STATUS_KEY] = 404;
    response_object[REASON_KEY] = msg;
    res = response_object.dump();

    if (send(client_fd, static_cast<const void *>(res.c_str()), res.size(), 0) == 0)
    {
        status = -1;
    }

    return status;
}

static int success_client_process(int client_fd)
{
    nlohmann::json response_object;
    std::string res;
    int status = 0;

    response_object[STATUS_KEY] = 200;
    response_object[REASON_KEY] = "Success";
    res = response_object.dump();

    if (send(client_fd, static_cast<const void *>(res.c_str()), res.size(), 0) == 0)
    {
        status = -1;
    }

    return status;
}

static void* process_listen_client(void *arg)
{
    int sock_client_fd = *((int *)arg);
    char msg_buf[MSG_SIZE];
    int status = 1;
    nlohmann::json json_object;
    int is_connect = true;

    memset(msg_buf, 0, MSG_SIZE);
    while (recv(sock_client_fd, msg_buf, MSG_SIZE, 0) != -1 && is_connect)
    {
        printf("client_msg: %s\n", msg_buf);
        json_object = nlohmann::json::parse(msg_buf);


        if (!json_object.contains(COMMAND_KEY))
        {
            std::cerr << "Json data from client " << sock_client_fd << " have not contain COMMAND KEY!" << std::endl;
            break;
        }

        switch (static_cast<int>(json_object[COMMAND_KEY]))
        {
            case REGISTER:
                if (json_object.contains(PASSWORD_KEY) && json_object.contains(USERNAME_KEY))
                {
                    status = register_client_process(json_object[USERNAME_KEY], json_object[PASSWORD_KEY], sock_client_fd);
                    if (status == -1)
                    {
                        abort_client_process(sock_client_fd, "Cannot register: this username already added!");
                        std::cerr << "Client " << sock_client_fd << " register abort: username alredy added!" << std::endl;
                    }
                    else
                    {
                        success_client_process(sock_client_fd);
                    }
                }
                else
                {
                    std::cerr << "Json data from client " << sock_client_fd << " have not contain PASSWORD OR USERNAME KEYS!" << std::endl;
                }
                break;
            
            case LOG_IN:
                if (json_object.contains(PASSWORD_KEY) && json_object.contains(USERNAME_KEY))
                {
                    status = log_in_client_process(json_object[USERNAME_KEY], json_object[PASSWORD_KEY], sock_client_fd);
                    if (status == -1)
                    {
                        status = abort_client_process(sock_client_fd, "Cannot log in: error in username or password!");
                        if (status == -1)
                        {
                            std::cerr << "Client " << sock_client_fd << ": cannot send response!" << std::endl;    
                        }
                        std::cerr << "Client " << sock_client_fd << " log in abort: error in username or password!" << std::endl;
                    }
                    else if (status == -2)
                    {
                        status = abort_client_process(sock_client_fd, "Cannot log in: client already logged!");
                        if (status == -1)
                        {
                            std::cerr << "Client " << sock_client_fd << ": cannot send response!" << std::endl;    
                        }
                        std::cerr << "Client " << sock_client_fd << " log in abort: client already logged" << std::endl;
                    }
                    else
                    {
                        status = success_client_process(sock_client_fd);
                        if (status == -1)
                        {
                            std::cerr << "Client " << sock_client_fd << ": cannot send response!" << std::endl;  
                        }
                    }
                }
                else
                {
                    std::cerr << "Json data from client " << sock_client_fd << " have not contain PASSWORD OR USERNAME KEYS!" << std::endl;
                }
                break;
            
            case MESSAGE:
                if (json_object.contains(MESSAGE_KEY))
                {
                    status = message_client_process(json_object[MESSAGE_KEY], sock_client_fd);
                    if (status == -1)
                    {
                        status = abort_client_process(sock_client_fd, "Falied send message: authorized first!");
                        if (status == -1)
                        {
                            std::cerr << "Client " << sock_client_fd << ": cannot send response!" << std::endl;  
                        }
                    }
                    else 
                    {
                        status = success_client_process(sock_client_fd);
                        if (status == -1)
                        {
                            std::cerr << "Client " << sock_client_fd << ": cannot send response!" << std::endl;  
                        }   
                    }
                }
                break;
            
            case DISCONNECT:
                status = disconnect_client_process(sock_client_fd);
                if (status == 0)
                {
                    std::cerr << "Client " << sock_client_fd << ": disconnect!" << std::endl;  
                }  
                else
                {
                    std::cerr << "Client " << sock_client_fd << ": failed write to file" << std::endl; 
                }
                is_connect = false;
                break; 
        }
        memset(msg_buf, 0, MSG_SIZE);
    }

    close(sock_client_fd);
    free_thread(pthread_self());

    return NULL;
}