#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore>
#include <future>
#include <cstdlib>
#include <random>
#include <queue>
#include <memory>
#include <condition_variable>

using namespace std;

mutex mut;
mutex dip;
mutex m;
condition_variable cv;

int lavori=20;
int dipendenti=5;


string type_task[20]={"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t"};

//classe task ha le informazioni necessarie per il promise e future anche alla costruione del lavoro che il dipendente deve fare
class task{

    public:

    size_t tempo_necessario;
    string tipo;
    double num1;
    double num2;

    promise<double> adding;
    future <double> finish_adding;

    task(size_t t,string type){

        tempo_necessario=t;
        tipo=type;

        finish_adding=adding.get_future();

    }

};

//fa il lavoro che gli è stato assegnato e se ne prende un altro in caso ce ne è uno libero
class worker{

    public:

    int ID_worker=0;
    shared_ptr<task> t;
    thread th;
    promise<double> final;
    mt19937 generatore;

    worker(int ID,queue <shared_ptr<task>>& task_generated):
    generatore(chrono::system_clock::now().time_since_epoch().count() + ID)
    {

        ID_worker=ID;

        {
            lock_guard<mutex> lock(dip);
            t=task_generated.front();
            
            task_generated.pop();
            
            th=thread(&worker::start_work,this,&task_generated);//evidenzia in base all'oggetto l'indirizzo della funzione
            
        }

    }

    void start_work(queue <shared_ptr<task>>* task_generated){

        while(true){

            {               
                lock_guard<mutex> lock(mut);
                cout<<"il lavoratore "<<ID_worker<<" sta iniziando la task di tipo "<<t->tipo<<" che dura "
                <<t->tempo_necessario<<" secondi\n\n";
            
                thread ta(&worker::adopera,this,&t->adding);
                ta.join();

            }

            this_thread::sleep_for(chrono::seconds(t->tempo_necessario));//simulo task da fare al worker

            {
                lock_guard<mutex> lock(mut);
                double f=t->finish_adding.get();

                cout<<"il thread con ID "<<ID_worker<<" ha finito il lavoro "<<t->tipo<<",esso era una somma e gli è durato "
                <<t->tempo_necessario<<" secondi"<<" i numeri erano "<<t->num1<<" e "<<t->num2<<" abbiamo ottenuto "<<f<<"\n\n";

                if(!get_new_work(*(&task_generated)))return;

            }   
            
        }

    }


    //funzione che permette a un worker di ottenere un lavoro disponibile al momento
    bool get_new_work(queue <shared_ptr<task>>* task_generated){

        {
            lock_guard<mutex> lock(dip);

            if((*task_generated).size()!=0){

                t=(*task_generated).front();
                cout<<"il lavoratore "<<ID_worker<<" ha deciso di prendere il lavoro di tipo "<<(*task_generated).front()->tipo<<"\n\n";
                (*task_generated).pop();

                return true;

            }

            dipendenti--;

            if(dipendenti==0){cv.notify_one();}
                       
            return false;
            
        }

    }   

    //controlla se il thread è terminabile
    void control_thread(){

        if(th.joinable()){

            th.join();

        }

    }

    //somma dei numeri
    void adopera(promise <double>* adding){
        
        {
            lock_guard<mutex> lock(dip);

            uniform_int_distribution<int> distribution(0, 99);
            

            t->num1 = distribution(generatore);
            t->num2 = distribution(generatore);
            adding->set_value(t->num1+t->num2); 
        }

    }

};

class master{

    public:

    vector <worker> workers;
    queue <shared_ptr<task>> task_generated;

    void create_workers(){

        workers.reserve(dipendenti);//fondamentale in quanto senza di esso il vettore durante i push_back cambierebbe posizioni
        //nell'heap nel bel mezzo dei thread ceh stanno prendendo le task e pescandone altri

        for(int i=0;i<dipendenti;i++){

            workers.emplace_back(i,task_generated);

        }

    }

    void create_tasks(){

        for(int i=0;i<lavori;i++){

            task_generated.push(make_shared<task>((rand()%10)+1,type_task[i]));
            

        }

    }

   void finish_threads(){

        for(auto& w : workers){

            w.control_thread();
        }

    }

};

int main(){

    srand(time(0));

    master M;

    //start del lavoro dei dipendenti
    M.create_tasks();
    M.create_workers();

    //while(dipendenti!=0){

    {
        unique_lock<mutex> lock(m);
        cv.wait(lock,[]{return (dipendenti==0)?true :false;});
    }
    
    
    //}

    cout<<"tutti i dipendenti hanno finito di lavorare\n";
    M.finish_threads();

    return 0;
}

/*
problemi affrontati per la miglioria del codice...

1)movimento del vettore nell'heap costante durante la selezione delle task per gli worker

per risolvere il problema ho utlizzato una funzione dei vettori chiamata 

nome.reserve(n);

indica al vettore di prendere in anticipo n celle di memoria per la allocazione di memoria questo permette al vettore di prendere uno spazio
totale gia dall'inizio senza che esso debba muoversi facendo dei push_back

2)utilizzo di smart pointers

utilizzati per evitare l'utlizzo di distruttori e togliere il pensiero sulla deallocazione di memoria(il controllo del funzionamento dei 
thread nche se la cella non è piu utlizzata dal queue è ancora con un controllo manuale)

è stato modificato anche la funzione che disabilita i thread con l'aggiunta di un & nelle parentesi rotonde,infatti senza di esso
non troverebbe gli effettivi oggetti collocati in memoria in quanto eliminati da soli a causa degli smart pointers.


*/
