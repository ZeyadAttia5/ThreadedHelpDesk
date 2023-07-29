#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
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

pthread_mutex_t mstudent_lock;

pthread_mutex_t mlock_print;

sem_t chair_semaphore;

uint8_t ta_exit = 0;
int students_helped = 0; // New counter to track the number of students helped

int main()
{

    sem_init(&chair_semaphore, 0, CHAIR_NUM);
    pthread_mutex_init(&lock_chairs, NULL);
    pthread_mutex_init(&mlock_print, NULL);
    pthread_cond_init(&is_student, NULL);
    pthread_cond_init(&cond_student, NULL);

    srand((unsigned int)time(NULL));

    // create ta
    pthread_t ta;
    pthread_attr_t ta_attr;
    pthread_attr_init(&ta_attr);
    TA teacher;
    teacher.tid = ta;
    teacher.status = SLEEPING;
    pthread_create(&teacher.tid, &ta_attr, ta_init, &teacher);
    Student * all_students[STUDENT_NUM];

    for (size_t i = 0; i < STUDENT_NUM; i++)
    {
        pthread_attr_t student_attr;
        pthread_t student_id;
        pthread_attr_init(&student_attr);
        Student *s = (Student *) malloc(sizeof(Student));
        s->number = i;
        s->student_id = student_id;
        s->status = PROGRAMMING;
        pthread_create(&s->student_id, &student_attr, student_init, s);
        all_students[i] = s;
    }
    ta_exit = 1;
    pthread_join(teacher.tid, NULL);
}

void *ta_init(void *teachr)
{
    TA *teacher = (TA *)teachr;
    while (1)
    {
        while (chairs[0].status == UNOCCUPIED)
        {
            teacher->status = SLEEPING;
            pthread_cond_wait(&is_student, &lock_chairs); // wait until a student comes and signals
        }

        Student *s = chairs[0].student;
        pthread_mutex_unlock(&lock_chairs); // was lockedfrom the sit()

        teacher->status = HELPING;
        int random_number = rand() % 3 + 1;
        sleep(random_number);             // help the Student
        pthread_mutex_lock(&lock_chairs); // released in the student_init
        // signal thread to exit
        pthread_mutex_lock(&mlock_exit); // unlocked in student_init()
        exit_flag = s->number;
        teacher->status = SLEEPING;
        students_helped++; // Increment the students_helped counter
        printf("student %i will be signaled with %i\n", s->number, exit_flag);
        pthread_cond_signal(&cond_student);
        pthread_join(chairs[0].student->student_id, NULL); // wait for the student to leave
        printf("Student %i exited\n", chairs[0].student->number);
        // if (ta_exit == 1 && students_helped == STUDENT_NUM)
        // {
        //     break;
        // }
    }
    return NULL;
}

void *student_init(void *stu)
{
    Student *s = (Student *)stu;
    int random_number = rand() % 3 + 1;
    sleep(random_number); // program for some period of time
    pthread_mutex_lock(&mlock_print);
    printf("student %i is programming\n", s->number);
    pthread_mutex_unlock(&mlock_print);

    do
    {
        // sem_wait(&chair_semaphore);
        int8_t chair_id = sit(s);
        if (chair_id == -1)
        {
            // come back later
            s->status = PROGRAMMING;
            // sem_post(&chair_semaphore);
            int random_number = rand() % 3 + 1;
            sleep(random_number);
        }
        else
        {
            break; // the student is now sitting
        }

    } while (1);

    pthread_mutex_lock(&lock_chairs); // released in ta_init
    pthread_cond_signal(&is_student); // wake up the teacher
    while (s->number != exit_flag)    // while the number was not signaled, wait
    {
        pthread_cond_wait(&cond_student, &lock_chairs); // wait until a signal comes to exit
    }
    printf("Student %i is awakened %i\n", s->number);
    vacate_chair(0);
    pthread_mutex_unlock(&lock_chairs); // lock was acquired by TA_init
    pthread_mutex_unlock(&mlock_exit);  // lock was acquired by TA_init
    pthread_exit(NULL);
}

// vacate the first chair and shift all students one unit to the left
void vacate_chair(int8_t chair_id)
{
    // signal thread to exit

    chairs[chair_id].status = UNOCCUPIED;
    chairs[chair_id].student->status = UNINITIALIZED;
    // sem_post(&chair_semaphore);

    // move next student to chairs[0] ensuring fairness --FIFO
    for (size_t i = 1; i < CHAIR_NUM; i++)
    {
        if (chairs[i].status != UNOCCUPIED && chairs[i - 1].status != UNOCCUPIED) // two consecutive uninitialized students means that there are no more left
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
            break;
        }
    }
    return retVal;
}

int8_t sit(Student *s)
{
    // use mutex or semaphore lock to edit the chairs array
    pthread_mutex_lock(&lock_chairs); // released in the ta_init
    int8_t chair_id = last_empty_chair();

    if (chair_id != -1)
    {
        chairs[chair_id].student = s;
        s->status = SITTING;
        chairs[chair_id].status = OCCUPIED;
    }
    pthread_mutex_unlock(&lock_chairs);
    return chair_id; // indicating success
}