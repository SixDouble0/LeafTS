#include <stdio.h>

#include "../hal/hal_vuart.h"

// WINSOCK2 - WINDOWS TCP SOCKET LIBRARY
#include <winsock2.h>
#include <ws2tcpip.h>

// PRAGMA TELLS LINKER TO LINK ws2_32.lib AUTOMATICALLY ON MSVC/MINGW
#pragma comment(lib, "ws2_32.lib")

// ACCEPTED CLIENT SOCKET - STATIC SO SEND/RECEIVE CAN ACCESS IT
static SOCKET g_client = INVALID_SOCKET;

// ------------------------------------------------------------------ //

// SEND DATA TO CONNECTED PYTHON CLIENT
static int vuart_send(const uint8_t *data, uint32_t len)
{
    if (g_client == INVALID_SOCKET) return HAL_UART_ERR;

    int sent = send(g_client, (const char *)data, (int)len, 0);
    return (sent > 0) ? HAL_UART_OK : HAL_UART_ERR;
}

// RECEIVE DATA FROM CONNECTED PYTHON CLIENT
// BLOCKS UP TO timeout_ms MILLISECONDS THEN RETURNS HAL_UART_TIMEOUT
static int vuart_receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    if (g_client == INVALID_SOCKET) return HAL_UART_ERR;

    // SET SOCKET RECEIVE TIMEOUT
    DWORD tv = (DWORD)timeout_ms;
    setsockopt(g_client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

    int received = recv(g_client, (char *)buf, (int)len, 0);

    if (received <= 0) return HAL_UART_TIMEOUT;
    return HAL_UART_OK;
}

// ------------------------------------------------------------------ //

// INITIALIZE VIRTUAL UART AS TCP SERVER
// CREATES SERVER SOCKET, BINDS TO PORT, WAITS FOR ONE CLIENT TO CONNECT
// BLOCKS UNTIL PYTHON CLIENT CONNECTS - THEN RETURNS
void vuart_init(hal_uart_t *hal, uint16_t port)
{
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);

    // CREATE TCP SERVER SOCKET
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // ALLOW PORT REUSE SO RESTART DOESN'T FAIL WITH "ADDRESS IN USE"
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind  (server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 1);

    printf("[vuart] Waiting for Python client on port %d...\n", port);

    // ACCEPT BLOCKS HERE UNTIL PYTHON APP CONNECTS
    g_client = accept(server, NULL, NULL);

    printf("[vuart] Client connected!\n");

    // SERVER SOCKET NO LONGER NEEDED - ONLY CLIENT SOCKET IS USED FROM NOW ON
    closesocket(server);

    // WIRE UP FUNCTION POINTERS - SAME PATTERN AS vflash_init
    hal->send    = vuart_send;
    hal->receive = vuart_receive;
}
