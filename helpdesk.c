#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <inttypes.h>
#include <semaphore.h>
#include <stdio.h>

#define STUDENT_NUM 10
#define CHAIR_NUM 3
enum ta_Status
{
    SLEEPING,
    HELPING,
};

typedef struct
{
    enum ta_Status status;
    pthread_t tid;
} TA;

enum student_status
{
    UNINITIALIZED = 0,
    SITTING,
    PROGRAMMING,
};

typedef struct
{
    enum student_status status;
    pthread_t student_id;
    uint8_t number;
} Student;

enum chair_status
{
    UNOCCUPIED,
    OCCUPIED,
};
typedef struct
{
    Student *student;
    enum chair_status status;
} Chair;

Chair chairs[CHAIR_NUM]; // editing to this array has to synchronized
void *ta_init(void *teacher);
void *student_init(void *s);
void vacate_chair(int8_t chair_id);
int8_t sit(Student *s);
int8_t last_empty_chair(void);

int8_t exit_flag = -1;
pthread_mutex_t mlock_exit;
pthread_cond_t cond_student;

pthread_cond_t is_student;
pthread_mutex_t lock_chairs;

pthread_mutex_t mlock_print;

int main()
{

    pthread_mutex_init(&lock_chairs, NULL);
    pthread_mutex_init(&mlock_print, NULL);
    pthread_cond_init(&is_student, NULL);
    pthread_cond_init(&cond_student, NULL);

    // for (int8_t i = 0; i < CHAIR_NUM; i++)
    // {
    //     printf("Student %i status is %i\n", chairs[i].student->status);
    // }

    // create ta
    pthread_t ta;
    pthread_attr_t ta_attr;
    pthread_attr_init(&ta_attr);
    TA teacher;
    teacher.tid = ta;
    teacher.status = SLEEPING;
    pthread_create(&teacher.tid, &ta_attr, ta_init, &teacher);

    pthread_attr_t student_attr;
    for (size_t i = 0; i < STUDENT_NUM; i++)
    {
        pthread_t student_id;
        pthread_attr_init(&student_attr);
        Student s;
        s.number = i;
        s.student_id = student_id;
        s.status = PROGRAMMING;
        pthread_create(&s.student_id, &student_attr, student_init, &s);
    }
    pthread_join(teacher.tid, NULL);
}

void *ta_init(void *teachr)
{
    TA *teacher = teachr;
    while (1)
    {
        while (chairs[0].status == UNOCCUPIED)
        {
            teacher->status = SLEEPING;
            pthread_cond_wait(&is_student, &lock_chairs); // wait until a student comes and signals
        }
        teacher->status = HELPING;
        pthread_mutex_unlock(&lock_chairs);
        sleep(1); // help the Student

        // signal thread to exit
        pthread_cond_signal(&is_student);
        // empty his chair
        printf("Student %i is leaving\n", chairs[0].student->number);
        vacate_chair(0);
        pthread_mutex_unlock(&lock_chairs);
        for (int8_t i = 0; i < CHAIR_NUM; i++)
        {
            printf("Chair %i has student %i\n", i, chairs[i].student->number);
        }
    }
}

void *student_init(void *stu)
{
    Student *s = stu;
    // if there are any chairs empty, sit

    int8_t come_back = 0;
    // pthread_mutex_lock(&mlock_print);
    // for (int8_t i = 0; i < CHAIR_NUM; i++)
    // {
    //     printf("Student %i is now sitting on chair %i\n", s->number, i);
    // }
    // pthread_mutex_unlock(&mlock_print);
    do
    {
        if (sit(s) != -1)
        {
            come_back = 0; // do not come back
        }
        else
        {
            come_back = 1;
            s->status = PROGRAMMING;
            printf("student %i will go program\n", s->number);
            sleep(4); // come back later;
        }
    } while (come_back == 1);

    while (s->number != exit_flag) // while the number was not signaled, wait
    {
        pthread_cond_wait(&is_student, &lock_chairs); // wait until a student comes and signals
    }
    pthread_exit(NULL);
}

// vacate the first chair and shift all students one unit to the left
void vacate_chair(int8_t chair_id)
{
    // signal thread to exit
    pthread_mutex_lock(&mlock_exit);
    exit_flag = chairs[0].student->number;
    pthread_cond_signal(&cond_student);
    pthread_mutex_unlock(&mlock_exit);

    pthread_join(chairs[chair_id].student->student_id, NULL);

    pthread_mutex_lock(&lock_chairs);
    chairs[chair_id].status = UNINITIALIZED;
    for (size_t i = 1; i < CHAIR_NUM; i++)
    {
        if (chairs[i].status != UNINITIALIZED && chairs[i - 1].status != UNINITIALIZED) // two consecutive uninitialized students means that there are no more left
        {
            chairs[i - 1] = chairs[i];
        }
        else
        {
            break;
        }
    }
}

// returns the id of the last empty chair
int8_t last_empty_chair()
{
    int8_t retVal = -1;
    for (size_t i = 0; i < CHAIR_NUM; i++)
    {
        if (chairs[i].status == UNOCCUPIED)
        {
            retVal = i; // chair is empty
        }
    }
    return retVal;
}

int8_t sit(Student *s)
{
    // use mutex or semaphore lock to edit the chairs array
    pthread_mutex_lock(&lock_chairs);
    int8_t chair_id = last_empty_chair();
    chairs[chair_id].student = s;
    s->status = SITTING;
    chairs[chair_id].status = OCCUPIED;
    // printf("%i\n", chair_id);
    pthread_mutex_lock(&mlock_print);
    // printf("Student %i is sitting on chair %i\n", s->number, chair_id);
    pthread_mutex_unlock(&mlock_print);

    pthread_cond_signal(&is_student);
    pthread_mutex_unlock(&lock_chairs);
    return chair_id; // indicating success
}