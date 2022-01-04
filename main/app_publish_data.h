#ifndef APP_PUBLISH_DATA_H
#define APP_PUBLISH_DATA_H

#define MAX_TOPIC_LEN 64

void publish_non_persistent_data(const char * topic, const char * data);
void publish_persistent_data(const char * topic, const char * data);

#endif // APP_PUBLISH_DATA_H