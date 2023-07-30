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

pthread_cond_t cond_student_arrived;
pthread_mutex_t mlock_student_arrived;

pthread_mutex_t mlock_chairs;

int students_helped = 0; // New counter to track the number of students helped
int students_left = 0;

int main()
{

    pthread_mutex_init(&mlock_chairs, NULL);

    pthread_mutex_init(&mlock_exit, NULL);
    pthread_cond_init(&cond_student, NULL);

    pthread_cond_init(&cond_student_arrived, NULL);
    pthread_mutex_init(&mlock_student_arrived, NULL);

    srand((unsigned int)time(NULL));

    // create ta
    pthread_t ta;
    pthread_attr_t ta_attr;
    pthread_attr_init(&ta_attr);
    TA teacher;
    teacher.tid = ta;
    teacher.status = SLEEPING;
    pthread_create(&teacher.tid, &ta_attr, ta_init, &teacher);
    Student *all_students[STUDENT_NUM];

    for (size_t i = 0; i < STUDENT_NUM; i++)
    {
        pthread_attr_t student_attr;
        pthread_t student_id;
        pthread_attr_init(&student_attr);
        Student *s = (Student *)malloc(sizeof(Student));
        s->number = i;
        s->student_id = student_id;
        s->status = PROGRAMMING;
        pthread_create(&s->student_id, &student_attr, student_init, s);
        all_students[i] = s;
    }
    pthread_join(teacher.tid, NULL);

    // free students' memory
    for (size_t i = 0; i < STUDENT_NUM; i++)
    {
        free(all_students[i]);
    }

    // destroy
    pthread_mutex_destroy(&mlock_exit);
    pthread_mutex_destroy(&mlock_student_arrived);
    pthread_mutex_destroy(&mlock_chairs);

    pthread_cond_destroy(&cond_student);
    pthread_cond_destroy(&cond_student_arrived);

    printf("Helped %i students\n", students_helped);
}

void *ta_init(void *teachr)
{
    TA *teacher = (TA *)teachr;
    while (1)
    {
        pthread_mutex_lock(&mlock_student_arrived);
        while (chairs[0].status == UNOCCUPIED)
        {
            teacher->status = SLEEPING;
            pthread_cond_wait(&cond_student_arrived, &mlock_student_arrived); // wait until a student comes and signals
        }

        Student *s = chairs[0].student;

        teacher->status = HELPING;
        int random_number = rand() % 1 + 1;
        sleep(random_number);                         // help the Student
        pthread_mutex_unlock(&mlock_student_arrived); // was locked from the sit()
        pthread_mutex_lock(&mlock_chairs);            // released in the student_init

        // signal thread to exit
        pthread_mutex_lock(&mlock_exit); // unlocked in student_init()
        exit_flag = s->number;
        teacher->status = SLEEPING;
        students_helped++;                 // Increment the students_helped counter
        pthread_mutex_unlock(&mlock_exit); // unlocked in student_init()
        pthread_cond_broadcast(&cond_student);
        pthread_join(chairs[0].student->student_id, NULL); // wait for the student to leave
        // printf("Student %i exited\n", chairs[0].student->number);
        if (students_helped == STUDENT_NUM)
        {
            break;
        }
    }
    return NULL;
}

void *student_init(void *stu)
{
    Student *s = (Student *)stu;
    int random_number = rand() % 3 + 1;
    sleep(random_number); // program for some period of time

    do
    {
        // sem_wait(&chair_semaphore);
        int8_t chair_id = sit(s);
        if (chair_id == -1)
        {
            // come back later
            s->status = PROGRAMMING;
            // sem_post(&chair_semaphore);
            int random_number = rand() % 2 + 1;
            printf("Student %i will come back later\n", s->number);
            sleep(random_number);
        }
        else
        {
            printf("Student %i sits\n", s->number);
            break; // the student is now sitting
        }

    } while (1);

    if (chairs[0].student->number == s->number)
    {
        pthread_cond_signal(&cond_student_arrived); // wake up the teacher
    }

    pthread_mutex_lock(&mlock_exit); // lock was acquired by TA_init
    while (s->number != exit_flag)   // while the number was not signaled, wait
    {
        pthread_cond_wait(&cond_student, &mlock_exit); // wait until a signal comes to exit
    }

    vacate_chair(0);
    printf("Student %i is leaving\n", s->number);
    students_left++;
    pthread_mutex_unlock(&mlock_exit);   // lock was acquired by TA_init
    pthread_mutex_unlock(&mlock_chairs); // lock was acquired by TA_init
    pthread_exit(NULL);
}

// vacate the first chair and shift all students one unit to the left
void vacate_chair(int8_t chair_id)
{
    // signal thread to exit

    // chairs[chair_id].status = UNOCCUPIED;
    chairs[chair_id].student->status = UNINITIALIZED;
    // sem_post(&chair_semaphore);

    // move next student to chairs[0] ensuring fairness --FIFO
    for (size_t i = 1; i < CHAIR_NUM; i++)
    {
        if (chairs[i].status == OCCUPIED && (i - 1) < CHAIR_NUM - 1) // two consecutive uninitialized students means that there are no more left
        {
            chairs[i - 1] = chairs[i];
        }
        else
        {
            break;
        }
    }
    chairs[CHAIR_NUM - 1].status = UNOCCUPIED;
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
    pthread_mutex_lock(&mlock_chairs); // released in the ta_init
    int8_t chair_id = last_empty_chair();

    if (chair_id != -1)
    {
        chairs[chair_id].student = s;
        s->status = SITTING;
        chairs[chair_id].status = OCCUPIED;
    }
    pthread_mutex_unlock(&mlock_chairs);
    return chair_id; // indicating success
}