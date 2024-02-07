#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>  

 //-----------------------------------------  Δομη παιδιου  ----------------------------------------------------------------------------------//
struct Child {
    int  name;//όνομα
    pid_t pid;//αναγνωριστικό
    int pipe_to_child[2];//διαυλος επικοινωνίας που παει στο παιδι
    int pipe_from_child[2];//διαυλος επικοινώνιας που φευγει απο το παιδί και παει στον πατερα
    int received_first_mess ;//μεταβλητή για να καταλαβαίνει ο πατερας αν είναι το πρώτο μηνυμα που θα στείλει ή οχι
};

  
 /* ----------------------------------------  ΥΛΟΠΟΙΗΣΗΣ ΛΙΣΤΑΣ   -------------------------------------------------------------------------------*/
  //δομη κομβου για λίστες
    struct Node {
    struct Child c;
    struct Node* next;
};
//Δομη λιστας
struct List {
    struct Node* head;
    struct Node* tail;
};


bool isEmpty(struct List* list) {//συναρτηση που ελεγχει αν ειναι άδεια η λιστα-ουρα
    return (list->head == NULL);
}

// Προσθήκη νέου κόμβου στο τέλος της λίστας
void add(struct List* list, struct Child child) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));//δυναμική δέσμευση μνημης για τον προστιθεμενο νέο κόμβο
    if (newNode == NULL) {//αν αποτύχει
        perror("malloc");
        exit(EXIT_FAILURE);
    }
//αρχικοποιηση του νέου κομβου
    newNode->c = child;
    newNode->next = NULL;
//διακρινω δυο περιπτώσεις 
    if (isEmpty(list)) {
        // Αν η λίστα είναι άδεια
        list->head = newNode;
        list->tail = newNode;
    } else {
        // Αν η λίστα δεν είναι άδεια
        list->tail->next = newNode;
        list->tail = newNode;
    }
}

// Αφαίρεση του πρώτου κόμβου από τη λίστα
void removeFirst(struct List* list) {
// Αν η λιστα ειναι αδεια δεν επιστρεφω κατι
//γιατι δεν υπαρχει κατι να διαγραψω
    if (isEmpty(list)) {
        return;
    }
//περνω το πρωτο στοιχείο της λιστας
    struct Node* temp = list->head;
    //αλλαζω τους δεικτες της κεφαλης
    //με το δευτερο στοιχείο
    list->head = list->head->next;
///απελευθερωσε την μνημη που δεσμευει το πρωτο στοιχειο
    free(temp);
}

// Επιστροφή του πρώτου κόμβου από τη λίστα
struct Child getFirst(struct List* list) {
  // Αν η λίστα είναι άδεια   
  if (isEmpty(list)) {
       
        fprintf(stderr, "Error: List is empty\n");
        exit(EXIT_FAILURE);
    }

    return list->head->c;
}

// Απελευθέρωση της μνήμης που καταλαμβάνεται από τη λίστα
void freeList(struct List* list) {
//περνουμε  το πρωτο στοιχειο
    struct Node* current = list->head;
//χρησιμοποιω την λογικη του iterator για διασχιση της λίστας
    while (current != NULL) {
    //αδειασμα των κομβων της λιστας
        struct Node* next = current->next;
        free(current);
        current = next;
    }
    //αδειαζω τη αρχη και το τελοσ της λιστας
    list->head = NULL;
    list->tail = NULL;
}
/*--------------------------------------------- Μεταβλητες που απελευθερωνει ο πατερας ------------------------------------------------------*/
  struct List relax ={NULL,NULL};//ουρα για παιδια που ειναι αδρανη
  struct List work ={NULL,NULL};//ουρα για παιδια που  εργαζονται
  struct Child* c ;//πινακας με τα παιδια που θα δημιουργηθουν δυναμικα
  int file_descriptor;
  int num_children=0;//πληθος παιδιων
 
 /*---------------------------------------------- ΥΛΟΠΟΙΗΣΗ ΒΟΗΘΗΤΙΚΩΝ ΣΥΝΑΡΤΗΣΕΩΝ  -----------------------------*/


 
void safe_write(int fd, const char *msg) {
    if (write(fd, msg, strlen(msg)) == -1) {
        perror("write");
        exit(1);
    }
}

void safe_read(int fd, char *buffer, size_t size) {
    ssize_t bytesRead = read(fd, buffer, size);
    if (bytesRead == -1) {
        perror("read");
        exit(1);
    }
}
 
  
  

/*------------------------------------- Χειρισμοσ Σηματων ---------------------------------------------*/


// Flag για τον έλεγχο του σήματος SIGINT
volatile sig_atomic_t terminate = 0;

// Signal handler για το σήμα SIGINT
void handle_sigint(int signum) {
   printf("\n\nDad receives ctrl+C");
        terminate = 1;//για να σταματησει η ατερμονη επικοινωνια
        kill(0,SIGTERM);
        // αν βαλουμε 0 στο ορισμα pid της kill στελνει 
        //σε ολες τις διεργασιες να τερματιστουν και στον εαυτο της
}

// Signal handler για το σήμα SIGTERM
void handle_sigterm_child(int signum){
printf("\nMy dad want to terminate me !!!");
exit(0);//τερματισμος παιδιου
}  
   
// Signal handler για το σήμα SIGTERM  που λαμβανει ο πατερας
void handle_sigterm_father(int signum) {
    for (int i = 0; i < num_children; i++) {
        // Περιμένει για την ολοκλήρωση κάθε παιδικής διεργασίας
        wait(NULL);
printf("\n free child's  %d pipes",i);
        // Κλείνει τα άκρα των σωληνώσεων καθε παιδιου
        close(c[i].pipe_to_child[0]);
        close(c[i].pipe_from_child[0]);
        close(c[i].pipe_to_child[1]);
        close(c[i].pipe_from_child[1]);

    }
   printf("\n free memory and source");
    // Ελευθερώνει τη μνήμη των λιστών
    freeList(&relax);
    freeList(&work);

    // Κλείνει το αρχείο
    close(file_descriptor);

    // Ελευθερώνει τη μνήμη του πίνακα δομών Child
    free(c);

    // Έξοδος από το πρόγραμμα
    exit(0);
}
  
  
  
  /*-------------------------------------------ΣΥΝΑΡΤΗΣΕΙΣ ΠΑΤΕΡΑ      -----------------------------------------------------*/  






//Αποστολη  μυνηματος στο παιδι
void sent_message_to_child(struct Child *c) { 
//κλεισιμο στο διαυλο που διαβαζει το παιδι
    close(c->pipe_to_child[0]);
     char mess[60];
     sprintf(mess, "Hello child %s, I am your father.", "");
     //εγγραφη στο διαυλο που γραφει ο πατερας
     safe_write(c->pipe_to_child[1], mess);
     //καθαρισμος buffer
     // memset(mess, 0, sizeof(mess));  
   
}

//Αποστολη δευτερου μηνυματος
void sent_second_message_to_child(struct Child *c) { 
  //κλεισιμο στο διαυλο που διαβαζει το παιδι
   close(c->pipe_to_child[0]);
     char mess[60];
     sprintf(mess, "I received done from chil %s\n.", "");
      //εγγραφη στο διαυλο που γραφει ο πατερας
     safe_write(c->pipe_to_child[1], mess);
   // memset(mess, 0, sizeof(mess));
}

//διαβασμα διαυλου απο πατερα
void parent_reads(struct Child *c) {
//κλεισιμο στο διαυλο που διαβαζει  ο πατερας
   close(c->pipe_from_child[1]);
    char buffer[60];
//διαβαζει ο πατερασ
    safe_read(c->pipe_from_child[0], buffer, sizeof(buffer));
    printf("parent reads: %s\n", buffer);
   // memset(buffer, 0, sizeof(buffer));
}





/*------------------------------------ΣΥΝΑΡΤΗΣΕΙΣ ΠΑΙΔΙΩΝ----------------------*/





// Το παιδι διαβαζει απο το διαυλο
void child_reads(struct Child *c ) {
//κλεισιμο διαυλου που δεν χρησιμοποιουμε
close(c->pipe_to_child[1]);
    char buffer[60];

    // διαβασμα του διαυλου
    safe_read(c->pipe_to_child[0], buffer, sizeof(buffer));

    printf("child  reads: %s\n",buffer);
    memset(buffer, 0, sizeof(buffer));
}

// Αποστολη μηνύματος στον πατερα
void child_write_pipe(struct Child *c ) {
//κλεισιμο διαυλου που δεν χρησιμοποιουμε
        close(c->pipe_from_child[0]);
        char buffer[60];
        //γραψιμο στο διαυλο 
        safe_write(c->pipe_from_child[1], "done\n");
        printf("Child writes: done\n");

    memset(buffer, 0, sizeof(buffer));
}

//κωδικα παιδικων διεργασιων
void child_process( struct Child *c, int file_descriptor, int i){

      //Παραμετροι  select
//δομή δεδομένων που χρησιμοποιείται  για να παρακολουθεί αν υπάρχουν διαθέσιμα δεδομένα
    fd_set child_rfds; 
    fd_set child_wfds; 
   //  δομή που χρησιμοποιείται για να οριστεί ο μέγιστος χρόνος αναμονής της συνάρτησης select
    struct timeval tm; 
    int retval;
    tm.tv_sec = 2;
    tm.tv_usec = 0;
    //ανιχνευση σηματος τερματισμου
    signal(SIGTERM ,(void (*)(int)) handle_sigterm_child);

    //κλεινω τα ακρα που δεν χρησιμοποιω κατα την παιδικη διεργασια
    close(c->pipe_to_child[1]);
    close(c->pipe_from_child[0]);
    
   
    //μεχρι να ληφθει το ctr c
    while (!terminate)
    {   
      
       
        //Αρχικοποίηση του child_rfds καθιστώντας το κενό. 
        FD_ZERO(&child_rfds);
        //Προσθήκη του file descriptor c->pipe_to_child[0] στο σύνολο child_rfds
        FD_SET(c->pipe_to_child[0], &child_rfds);
      /*κοιτα  (fd pipe_to_child[0]) αν  εχει δεδομενα απο τον πατερα */
        retval = select(FD_SETSIZE, &child_rfds, NULL, NULL, &tm);
        if (retval == -1){
            perror("select()");}
        else if (retval){
        //οταν εχει σταλθει κατι
            printf("Child<%d> can read the message from parent.\n",c->name);
            child_reads(c);
            dprintf(file_descriptor, "pid%d --> child%d\n", c->pid, i);
           sleep((i%2+1));
            child_write_pipe(c);
          
        }else{
          //  printf("Child<%d> cannot read the message from parent.\n",c->name);
        
     }
    }
     
     
    //κλεινω τα ακρα  που χρησιμοποιησα
    close(c->pipe_to_child[0]);
    close(c->pipe_from_child[1]);
   //τερματισμος
    exit(0);
        }




/* ---------------------------------- ΜΑΙΝ ΣΥΝΑΡΤΗΣΗ    -------------------*/




int main(int argc , char *argv[]){

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_children> <file> \n", argv[0]);
        return 1; // Επιστρέφουμε 1 για να υποδηλώσουμε λάθος
    }


     num_children = atoi(argv[1]);
     char *filename = argv[2];
     
   // Δημιουργία πίνακα δομών Child με χρήση malloc
       c = (struct Child*)malloc(num_children * sizeof(struct Child));
     if (c == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
}


//δημιουργια διαυλων επικοινωνιας
 for(int i = 0 ; i < num_children ; i++){
        if (pipe(c[i].pipe_to_child) == -1 || pipe(c[i].pipe_from_child) == -1) {
            perror("pipe");
            exit(1);
        }
      
}

for(int i=0;i<num_children;i++){
  //αρχικοποιηση παιδιων
       c[i].name = i;
      c[i].received_first_mess = 0;
 //προσθηκη στην ουρας με τις διεργασιες που ειναι αδρανης     
        add(&relax, c[i]);
}

 // Ανιχνευση του signal handler για το SIGINT
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }
    
    //Ανιχνευση  του signal handler για το SIGINT
    if (signal(SIGTERM, handle_sigterm_father) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }

 file_descriptor = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (file_descriptor == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    lseek(file_descriptor, 0, SEEK_SET);

      //Παραμετροι  select
//δομή δεδομένων που χρησιμοποιείται  για να παρακολουθεί αν υπάρχουν διαθέσιμα δεδομένα
    fd_set parent_rfds; 
    fd_set parent_wfds; 
//  δομή που χρησιμοποιείται για να οριστεί ο μέγιστος χρόνος αναμονής της συνάρτησης select    
    struct timeval time; 
    int retval;
    time.tv_sec = 3;
    time.tv_usec = 0;


  
    pid_t fork_pid;
    for (int i = 0; i < num_children; i++)
    {
    //Δημιουργια παιδιων
        fork_pid = fork();
        if (fork_pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        
      //  printf("loop child: %d\n\n",i);
      
        if (fork_pid > 0) {  
            // πατερας
        } else { 
        //παιδια
         //αποθηκευση του pid στην παιδικη διεργασια
        c[i].pid= getpid();
        //διεργασια για λειτουργιες παιδιου
       child_process( &c[i],file_descriptor,i);
     }
    }
     
 
   
//Κωδικας πατερα

//μεχρι να τερματιστει απο σημα ctrl+C 
    while (!terminate)
        {
        
            sleep(1);
          //εαν υπαρχει καποιο διαθεσιμο παιδι για να στειλεις μηνυμα
          if(!isEmpty(&relax)){
                //πρωτο διαθεσιμο παιδι
                struct Child c = getFirst(&relax);
                //διαγραφη απο την ουρα
                removeFirst(&relax);
              //αρχικοποιηση σε κενο της δομης  parent wfds
                FD_ZERO(&parent_wfds);
             //Προσθήκη του file descriptor c->pipe_to_child[1] στην δομη
                FD_SET(c.pipe_to_child[1], &parent_wfds);
             //αν υπαρχει καποιο διαθεσιμο  μηνυμα για να διαβασει
                retval = select(FD_SETSIZE, NULL, &parent_wfds, NULL, &time);
              
               if (retval == -1){
                    perror("select()");
                }else if (retval){
                     if(c.received_first_mess == 0){
                     //Την πρωτη φορα που στελνει το μηνυμα
                     printf("Parent can write to the child<%d>.\n",c.pid);
                     //προσθηκη στην ουρα μετα απασχολημενα παιδια
                     add(&work,c);
                     sent_message_to_child(&c);
                     c.received_first_mess = 1;
                  }else{
                  //την δευτηερη φορα
                     printf("Parent can write to the child<%d>.\n",c.pid);
                     add(&work,c);
                     sent_second_message_to_child(&c);
                     }
                }else{
                  //  printf("Parent cannot write from the child<%d>.\n",c.pid);
                } 
           }
           
           
                sleep(1);
                
           //αν υπαρχει στην ουρα  καποιο παιδι που του εχει σταλθει να διαβασει     
          if(!isEmpty(&work))  {   
          //πρωτο  παιδι στην ουρα που εχει γραψει
                struct Child cr = getFirst(&work);
          // αφαιρεση απο την ουρα 
                removeFirst(&work);
            //αρχικοποιηση σε κενο της δομης  parent rfds    
                FD_ZERO(&parent_rfds);
            //Προσθήκη του file descriptor c->pipe_from_child[0] στην δομη  
                FD_SET(cr.pipe_from_child[0], &parent_rfds);
       //αν υπαρχει καποιο διαθεσιμο  μηνυμα για να διαβασει
                retval = select(FD_SETSIZE, &parent_rfds, NULL, NULL, &time);
              

                if (retval == -1)
                    perror("select()");
                else if (retval){
                //αν λαβει κατι τοτε διαβασε το
                    printf("Parent can read from the child<%d>.\n",cr.pid);
                    parent_reads(&cr);
                    add(&relax ,cr);//το παιδι μπαινει στην ουρα  των παιδιων σε αναμονη
                } else{
                 //   printf("Parent cannot read from the child<%d>.\n",cr.pid);
                   
                     }
            }

      }  
        

    for (int i = 0; i < num_children; i++)
    {
        //κλεισιμο των διαυλων
        close(c[i].pipe_from_child[1]);
        close(c[i].pipe_to_child[1]);
    }
 

    // Αναμονή για τον τερματισμό όλων των παιδιών
    for (int i = 0; i < num_children; i++) {
        wait(NULL);
    }

printf("\n clean the source");
//απελευθερωση δεσμευσης μνημης
    freeList(&relax);
    freeList(&work);
    close(file_descriptor);
    free(c);
    return 0;
}




