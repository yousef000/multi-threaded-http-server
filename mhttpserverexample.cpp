#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int counter = 0; 

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_cond = PTHREAD_COND_INITIALIZER;

#define COUNT_DONE 10
#define COUNT_HALT1  3
#define COUNT_HALT2  6

void *print_message(void *ptr){
    pthread_mutex_lock(&mutex1);
    char *message;
    counter++;
    message = (char *) ptr;
    printf("%s %d\n", message, counter);
    pthread_mutex_unlock(&mutex1);

}
void *functionCount1(void *ptr){
    for(;;){
        pthread_mutex_lock(&condition_mutex);
        while (counter >= COUNT_HALT1 && counter <= COUNT_HALT2)
        {
            pthread_cond_wait(&condition_cond, &condition_mutex);
        }
        pthread_mutex_unlock(&condition_mutex);

        pthread_mutex_lock(&mutex1);
        counter++;
        printf("Counter value functionCount1: %d\n", counter);
        pthread_mutex_unlock(&mutex1);

        if(counter >= COUNT_DONE) return(NULL);
    }
    
}
void *functionCount2(void *ptr){
    for(;;){
        pthread_mutex_lock(&condition_mutex);
        if(counter < COUNT_HALT1 || counter > COUNT_HALT2)
        {
            pthread_cond_signal(&condition_cond);
        }
        pthread_mutex_unlock(&condition_mutex);

        pthread_mutex_lock(&mutex1);
        counter++;
        printf("Counter value functionCount2: %d\n", counter);
        pthread_mutex_unlock(&mutex1);

        if(counter >= COUNT_DONE) return(NULL);
    }
    
    
}
int main(){
    pthread_t thread1, thread2, thread3, thread4;
    char *message1 = "Counter value printmessage1:";
    char *message2 = "Counter value print_message2:";

    pthread_create(&thread1, NULL, print_message, (void*) message1);
    pthread_create(&thread2, NULL, print_message, (void*) message2);
    
    pthread_create(&thread4, NULL, &functionCount2, NULL);
    pthread_create(&thread3, NULL, &functionCount1, NULL);
    pthread_join(thread3, NULL);
    pthread_join(thread4, NULL);
    
    return 0;
}