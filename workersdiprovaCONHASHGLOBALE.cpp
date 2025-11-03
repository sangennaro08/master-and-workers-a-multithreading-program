#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <future>
#include <cstdlib>
#include <random>
#include <queue>
#include <memory>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <unordered_map>

using namespace std;

mutex tasks_finished;
mutex do_tasks;
mutex wait_ending_workers;

condition_variable continue_the_main;

atomic<int> tasks_required=20;
atomic<int> workers_doing_tasks=30;

unordered_map<string,double> complited_tasks;

vector<string> type_task;

//classe task ha le informazioni necessarie per il promise e future anche alla costruione del lavoro che il dipendente deve fare
class task{

    public:

    size_t do_task_time;
    string type_of_work;
    double num1;
    double num2;

    promise<double> adding;
    future <double> finish_adding;
    

    task(size_t t,string type){

        do_task_time=t;
        type_of_work=type;

        finish_adding=adding.get_future();

    }

};

//fa il lavoro che gli è stato assegnato e se ne prende un altro in caso ce ne è uno libero
class worker{

    private:

    int ID_worker;
    shared_ptr<task> task_given;
    thread th;
    mt19937 generator;
    
    public:

    worker(int ID,queue <shared_ptr<task>>& task_generated):
    generator(chrono::system_clock::now().time_since_epoch().count() + ID)
    {

        ID_worker=ID;

        {
            lock_guard<mutex> lock(do_tasks);

            if(!task_generated.empty()){

            task_given=task_generated.front();
            
            task_generated.pop();
            
            th=thread(&worker::start_work,this,&task_generated);//evidenzia in base all'oggetto l'indirizzo della funzione

            }
            
        }

    }

    void start_work(queue <shared_ptr<task>>* task_generated){
        
        while(true){

            {               
                lock_guard<mutex> lock(tasks_finished);
                cout<<"il lavoratore "<<ID_worker<<" sta iniziando la task di tipo "<<task_given->type_of_work<<" che dura "
                <<task_given->do_task_time<<" secondi\n\n";
            
                adopera(&task_given->adding);

            }

            this_thread::sleep_for(chrono::seconds(task_given->do_task_time));//simulo task da fare al worker

            {
                lock_guard<mutex> lock(tasks_finished);
                double result=task_given->finish_adding.get();

                cout<<"il thread con ID "<<ID_worker<<" ha finito il lavoro "<<task_given->type_of_work<<",esso era una somma e gli è durato "
                <<task_given->do_task_time<<" secondi"<<" i numeri erano "<<task_given->num1<<" e "<<task_given->num2<<" abbiamo ottenuto "
                <<result<<"\n\n";
                complited_tasks[task_given->type_of_work]=result;

                if(!get_new_work(*(&task_generated)))return;

            }
                       
        }

    }


    //funzione che permette a un worker di ottenere un lavoro disponibile al momento
    bool get_new_work(queue <shared_ptr<task>>* task_generated){

        {
            lock_guard<mutex> lock(do_tasks);

            if(!(*task_generated).empty()){

                task_given=(*task_generated).front();
                cout<<"il lavoratore "<<ID_worker<<" ha deciso di prendere il lavoro di tipo "<<(*task_generated).front()->type_of_work<<"\n\n";
                (*task_generated).pop();

                return true;

            }

            workers_doing_tasks--;

            if(!workers_doing_tasks){continue_the_main.notify_one();}
            return false;
            
        }
        
    }   

    //controlla se il thread è terminabile
    void control_thread(){

        if(th.joinable())th.join();

    }

    //somma dei numeri
    void adopera(promise <double>* adding){

        uniform_int_distribution<int> distribution(0, 99);//indichiamo un numero che verrà generato tra 0 e 99
            
        task_given->num1 = distribution(generator);//generare numero da 0 a 99
        task_given->num2 = distribution(generator);

        adding->set_value(task_given->num1 + task_given->num2);//mandare risultato a result nel mentre che aspetta

    }

};

class master{

    public:

    vector <worker> workers;
    queue <shared_ptr<task>> task_generated;

    void create_workers(){

        if(workers_doing_tasks>=tasks_required){

            int diminuire=workers_doing_tasks-tasks_required;
            workers_doing_tasks-=diminuire;

            cout<<"dipendenti diminuiti a "<<workers_doing_tasks<<" in quanto sono stati inseriti di più di quanto necessari\n\n";

        }


        workers.reserve(workers_doing_tasks);//fondamentale in quanto senza di esso il vettore durante i push_back cambierebbe posizioni
        //nell'heap nel bel mezzo dei thread ceh stanno prendendo le task e pescandone altri

        for(int i=0; i<workers_doing_tasks; i++){
                        
            workers.emplace_back(i,task_generated);
            
        }
    }    

    void create_tasks(){

        for(int i=0; i<tasks_required; i++){

            task_generated.push(make_shared<task>((rand()%10)+1,type_task[i]));
            
        }

    }

    void name_tasks(){

        type_task.reserve(tasks_required);

        for(int i=0;i<tasks_required;i++){

            type_task.push_back("A"+to_string(i));

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

    master M;//allocato nello stack

    //start del lavoro dei workers_doing_tasks
    M.name_tasks();
    M.create_tasks();
    M.create_workers();

    {
        unique_lock<mutex> lock(wait_ending_workers);

        continue_the_main.wait(lock,[]{return !workers_doing_tasks;});
    }
        
    cout<<"tutti i dipendenti hanno finito di lavorare\n";
    
    cout<<"risultati ottenuti per ogni task completata:\n\n";

    for(auto& tasks_completed : complited_tasks){

        cout<<tasks_completed.first<<" : "<<tasks_completed.second<<"\n";

    }

    M.finish_threads();//i thread dentro worker vengono joinati

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

