#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore>

using namespace std;

class task;

mutex mut;
mutex dip;
//variabili globali 
int lavori=20;
int dipendenti=5;

string type_task[20]={"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t"};

//classe task con i suoi rispettivi attributi
class task{
    
    public:
    size_t tempo_necessario;
    string tipo;

    task(size_t t,string type){

        tempo_necessario=t;
        tipo=type;

    }

};

//worker che ha come compito principale quello di lavorare i processi da loro scelti dal queue di task
class worker{
    
    private:

    thread th;
    task* t;
    int ID_worker=0;

    public:
    //creazione thread che lavora    
    worker(int ID,vector <task*>& task_generated){
        
        ID_worker=ID;  
        
        {

            lock_guard<mutex> lock(dip);
            t=task_generated.at(task_generated.size()-1);
            task_generated.pop_back();

            th=thread(&worker::work_task,this,&task_generated);//errore per refence dandling meglio farlo con puntatore come fatto qua

        }
        
        
        
    }

    ~worker(){delete t;}

    //il thread lavora le tasks notare il doppio puntatore per evitare refence dandling
    
    void work_task(vector <task*>* task_generated){
        
        while(true){

                {
                    lock_guard<mutex> lock(mut);
                    cout<<"il lavoratore "<<ID_worker<<" sta iniziando la task di tipo "<<t->tipo<<" che dura "
                    <<t->tempo_necessario<<" secondi\n\n";
                }
    
                this_thread::sleep_for(chrono::seconds(t->tempo_necessario));//simulo task da fare al worker
       
                {
                    lock_guard<mutex> lock(mut);
                    
                    cout<<"il lavoratore "<<ID_worker<<" ha completato la task di tipo "<<t->tipo<<" che è durato "
                    <<t->tempo_necessario<<" secondi\n\n";

                    bool controllo=get_new_task(&task_generated);

                    if(!controllo)return;
                }

                
            
        }
        
    }

void join_thread() {

    if (th.joinable()) {

        th.join();

    }

}

bool get_new_task(vector <task*>** task_generated){

        //se il thread vede che ci sono altre tasks prende quella per ultima inserita e viene tolta(tolto dall'utilizzo ma non dinamicamente)
        {

        lock_guard<mutex> lock(dip);//dip e non mut così evito overlapping dei testi in uscita

            if((**task_generated).size()!=0){

                
                
                t=(**task_generated).at((**task_generated).size()-1);
                cout<<"il lavoratore "<<ID_worker<<" ha deciso di prendere il lavoro di tipo "<<(**task_generated).at((**task_generated).size()-1)->tipo<<"\n\n";
                (**task_generated).pop_back();
                
                return true;
                

            }

            dipendenti--;
            return false;
        }

    }
    
};

//ha sotto controllo i lavoratori che li genera assieme alle task(ha come compito anche quello di congedare i dipendenti)
class master{
    
    public:

    vector <worker*> tot_dipendenti;
    vector <task*> task_generated;

    //-----------------------------------creazioni tasks e workers-----------------------------------
    void create_tasks(){
        
        for(int i=0;i<lavori;i++){

            
            task_generated.emplace_back(new task((rand()%10)+1,type_task[i]));
            
        }
        
    }
    
    void create_workers(){
        
        for(int i=0;i<dipendenti;i++){
            
            tot_dipendenti.emplace_back(new worker(i, task_generated));//reso tutte le variabili di worker visto che non sono utlizzate dall'esterno

        }
        
    }

    //libera la memoria allocata dinamicamente
    void delete_workers(){

        for(auto w : tot_dipendenti){

            w->join_thread();
            delete w;

        }
    }
    
};

int main()
{
    srand(time(nullptr));
    
    master M;
    
    //inizializzazione dipendenti e lavori
    M.create_tasks();
    M.create_workers();
    
    while(true){
        
        {
            
            lock_guard<mutex> lock(dip);
            
            if(dipendenti==0)break;
            
        }
        
    }
    
    cout<<"tutti i lavoratori hanno finito di lavorare\n";

    M.delete_workers();

    return 0;
}

/*
cosa ho scoperto?
scrivere 

t->tipo e *t.tipo infatti

t->tipo=*t.tipo

infatti la freccia deferenzia per arrivare alla zona effettiva in cui si trova il vector

QUINDI 

vector <task*>** task_generated 

se volessi masi accedere al suo interno faccio in 2 modi diversi

1)

(**task_generated). ...

oppure 

2)

*task_generated-> ...

per far passare a una funzione un puntatore per renderlo un double pointer fare

vector <task*>** task_generated

func(&task_generated)

poi fare

void func(vector <task*>** task_generated){...}

quando accade il dangling reference

dangling reference:accesso a una cella di memoria non valida o deallocata,senza alcun dato utile al programma dato.

era un problema in questo esercizio visto che ref che crea un collegamento al vector effettivo SE casualmente ci fosse stato un worker 
appena nato nel costruttore e faceva un pop_back allora la zona puntata diventava non valida

se facciamo &task_generated e non ref(task_generated) il problema cade in quanto anche se viene tolto quell'elemento STIAMO 
PUNTANDO DIRETTAMENTE ALLA CELLA senza variabili intermediarie che è caso del ref(...)

quindi se mai noi lavoriamo con dei vector stare sempre attenti con 


*/