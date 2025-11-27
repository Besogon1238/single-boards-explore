# Идеи применения одноплатников

В данном подразделе планируется фиксировать всевозможные идеи(а может и реализацию) практического применения одноплатников в учебных целях.

<a name="mqtt-client"></a>


## Оглавление
- [Использование в качестве MQTT-клиента](#mqtt-client)


## Использование в качестве MQTT-клиента/брокера

MQTT (Message Queuing Telemetry Transport) – это легковесный протокол для обмена сообщениями между устройствами, работающий по модели «издатель-подписчик» (pub/sub). Разработан в 1999 году для мониторинга нефтепроводов, но сегодня широко используется в IoT, умных домах, промышленной автоматизации и других сферах, где важны низкое энергопотребление и работа в нестабильных сетях.

### Основные компоненты

- **Брокер (Broker)** — центральный сервер, который принимает сообщения от **издателей (publishers)**, пересылает их **подписчикам (subscribers)** по нужным топикам.

- **Клиенты** — устройства или приложения которые могут обладать следующими ролями(причем иногда одновременно)
    - **Издатель** — отправляет данные в брокер
    - **Подписчик** — получает данные по подписке
- **Топики** — адреса, по которым передаются сообщения. Иерархическая структура через **/**: 
    - *home/kitchen/temperature*
    - *factory/machine1/status*

Более подробную информацию о протоколе и его возможностях, например про шаблоны подписки и качество обслуживания, предлагаю читателю найти самостоятельно :)

Данный протокол широко применяется в умных домах, различных системах мониторинга и телеметрии.

### Пример реализации

Идея применения одноплатников путем использования проста — к одноплатникам можно подключить различные датчики, обрабатывать с них данные, а затем уже отправлять по подписке брокеру и другим клиентам для дальнейшей обработки и визуализации на более мощной машине.

Приведу самую простую реализацию издателя на C (для одноплатника) и брокера-подписчика на Python (для ПК). 

Вводные прежние: есть одноплатник lichee_rv с Альтом на борту, и x86_64  ПК.

Для написания клиента нам пригодиться библиотека paho-c, разработанную для реализации клиентской части протокола MQTT. Для установки на клиенте необходимо выполнить на одноплатнике:

```
# apt-get install libpaho-mqtt1 libpaho-mqtt-devel
```

Для написания брокера, выполняющего одновременно функцию подписчика будем использовать питоновскую реализацию paho.

```
# apt-get install python3-module-paho
```

В моем примере брокер-подписчик, выступая в качестве брокера в отдельном потоке высылает в свою локальную сеть бродкаст-запросы, чтобы дать возможность клиентскому устройству понять, куда ему подключаться.

Выступая же в качестве клиента он подписывается на топик *lichee_rv/stats*, и при получении данных просто выводит их в консоль.

<details>

<summary>Вспомогательные функции брокера-подписчика</summary>

<pre><code>
import json
import time
import socket


def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    client.subscribe("lichee_rv/stats")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        print(f"Raw message received: {payload}")
        data = json.loads(payload)
        print(f"Parsed data: {data}") 
    except Exception as e:
        print(f"Error processing message: {e}")


def get_local_ip():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.connect(("8.8.8.8", 80))  # Google DNS
        ip = sock.getsockname()[0]
        sock.close()
        return ip
    except Exception as e:
        print(f"Error getting IP: {e}")
        return "127.0.0.1"


def broadcast_broker_ip():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    
    BROADCAST_PORT = 54545

    current_ip = get_local_ip()
    message = f"MQTT_BROKER:{current_ip}:1883".encode()
    
    while True:
        try:
            sock.sendto(message, ('255.255.255.255', BROADCAST_PORT))
            print(f"[UDP] Sent broadcast: {message.decode()}")
        except Exception as e:
            print(f"[UDP] Broadcast error: {e}")
        time.sleep(5) 

</code></pre>

</details>


<details>

<summary>Главная функция брокера-подписчика</summary>

<pre><code>
import threading
import paho.mqtt.client as mqtt
from broker_utils import on_connect,on_message,get_local_ip,broadcast_broker_ip

threading.Thread(target=broadcast_broker_ip, daemon=True).start()

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

broker_ip = get_local_ip()

try:
    client.connect(broker_ip, 1883, 60)
    print("Connecting to broker...")
    client.loop_forever()
except KeyboardInterrupt:
    client.disconnect()
    print("Disconnected")
except Exception as e:
    print(f"Connection error: {e}")

</code></pre>


</details>

В свою очередь клиент-издатель, ищет в сети брокера, подключается к нему. После чего вычисляет текущую нагрузку на ЦП и использование оперативной памяти(ну а что еще измерять, если нету датчиков) и отправляет с нужным топиком.


<details>

<summary>Главная функция клиента-издателя</summary>

<pre><code>

#include <stdio.h>
#include "client_utils.h"

int main() {

    char* broker_ip = discover_broker();
    if (!broker_ip) {
        fprintf(stderr, "Failed to discover broker\n");
        return 1;
    }

    printf("Discovered broker at %s\n", broker_ip);

    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (mosquitto_connect(mosq, broker_ip, MQTT_PORT, KEEPALIVE)) {
        fprintf(stderr, "MQTT connection failed\n");
        free(broker_ip);
        return 1;
    }

    char payload[128];
    float cpu, ram;

    mosquitto_lib_init();

    if (!mosq) {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    if (mosquitto_connect(mosq, broker_ip, MQTT_PORT, KEEPALIVE)) {
        fprintf(stderr, "Unable to connect to MQTT broker.\n");
        return 1;
    }

    free(broker_ip);

    printf("MQTT client started.\n");

    while (1) {
        cpu = get_cpu_usage();
        ram = get_ram_usage();

        if (cpu < 0 || ram < 0) {
            fprintf(stderr, "Error reading system stats\n");
            sleep(1);
            continue;
        }

        snprintf(payload, sizeof(payload),"{\"cpu\":%.2f,\"ram\":%.2f}", cpu, ram);

        int ret = mosquitto_publish(mosq, NULL, MQTT_TOPIC,strlen(payload), payload, 0, false);

        if (ret != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(ret));
        } else {
            printf("Sent: %s\n", payload);
        }

        sleep(1);
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}


</code></pre>

</details>


<details>
<summary>Вспомогательные функции клиента-издателя</summary>

<pre><code>
#ifndef CLIENT_H
#define CLIENT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mosquitto.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BROADCAST_PORT 54545
#define BROADCAST_MAGIC "MQTT_BROKER:"

#define MQTT_PORT 1883
#define MQTT_TOPIC "lichee_rv/stats"
#define KEEPALIVE 60
#endif

static char* discover_broker() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket error");
        return NULL;
    }

    // Allow broadcast messages
        int broadcast_enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

    // Configure reciever address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("UDP bind error");
        close(sock);
        return NULL;
    }

    printf("Listening for broker broadcasts...\n");
    char buffer[256];
    int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
    close(sock);

    if (len <= 0) {
        perror("UDP receive error");
        return NULL;
    }

    buffer[len] = '\0';
    printf("Received broadcast: %s\n", buffer);

    // Verify that messagw from broker
    if (strstr(buffer, BROADCAST_MAGIC) != buffer) {
        fprintf(stderr, "Invalid broadcast message\n");
        return NULL;
    }

    // Parsing ip and port from format "MQTT_BROKER:IP:PORT")
    char* ip = buffer + strlen(BROADCAST_MAGIC);
    char* port_str = strchr(ip, ':');
    if (!port_str) {
        fprintf(stderr, "Invalid broadcast format\n");
        return NULL;
    }

    *port_str = '\0';  // Split ip and port
    int port = atoi(port_str + 1);

    return strdup(ip);
}

static float get_cpu_usage() {
    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) return -1;

    unsigned long user, nice, system, idle;
    fscanf(fp, "cpu %lu %lu %lu %lu", &user, &nice, &system, &idle); // read cpu info
    fclose(fp);

    unsigned long total = user + nice + system + idle;
    static unsigned long prev_total = 0, prev_idle = 0;

    float usage = 0.0;
    if (prev_total > 0) {
        float diff_idle = idle - prev_idle;
        float diff_total = total - prev_total;
        usage = 100.0 * (1.0 - diff_idle / diff_total); // calculate cpu usage
    }

    prev_total = total;
    prev_idle = idle;
    return usage;
}

static float get_ram_usage() {
    FILE* fp = fopen("/proc/meminfo", "r");
    if (!fp) return -1;

    char line[128];
    unsigned long total = 0, free = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "MemTotal:")) sscanf(line, "MemTotal: %lu kB", &total);
        if (strstr(line, "MemFree:")) sscanf(line, "MemFree: %lu kB", &free);
    }
    fclose(fp);

    if (total == 0) return -1;
    return 100.0 * (total - free) / total;
}
</code></pre>
</details>

[Далее: Создание компонентов образа](/Subpages/Create_images_prep.md)