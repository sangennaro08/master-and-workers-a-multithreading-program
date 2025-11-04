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

using namespace std;

mutex tasks_finished;
mutex do_tasks;
mutex wait_ending_workers;

condition_variable continue_the_main;

atomic<int> tasks_required=20;
atomic<int> workers_doing_tasks=5;

//nome tasks
vector<string> type_task;

//classe task ha le informazioni necessarie per il promise e future anche alla costruione del lavoro che il dipendente deve fare
class Task{

    public:

    size_t do_task_time;
    string type_of_work;
    double num1;
    double num2;
    double result;

    promise<double> adding;
    future <double> finish_adding;

    Task(size_t t,string type){

        do_task_time=t;
        type_of_work=type;

        finish_adding=adding.get_future();

    }

};

//fa il lavoro che gli è stato assegnato e se ne prende un altro in caso ce ne è uno libero
class Worker{

    private:

    int ID_worker;
    vector<shared_ptr<Task>> task_given;
    thread th;
    mt19937 generatore;
    
    public:

    Worker(int ID,queue <shared_ptr<Task>>& task_generated):
    generatore(chrono::system_clock::now().time_since_epoch().count() + ID),ID_worker(ID)
    {

        {
            lock_guard<mutex> lock(do_tasks);

            if(!task_generated.empty()){

            task_given.push_back(task_generated.front());
            
            task_generated.pop();
            
            th=thread(&Worker::start_work,this,&task_generated);//evidenzia in base all'oggetto l'indirizzo della funzione

            }
            
        }

    }
    //-----------------------------------funzione che simula il lavoro di ogni worker-----------------------------------//
    void start_work(queue <shared_ptr<Task>>* task_generated){
        
        while(true){

            //lock guard dove chiamiamo una funzione per fare una somma
            {               
                lock_guard<mutex> lock(tasks_finished);
                cout<<"il lavoratore "<<ID_worker<<" sta iniziando la task di tipo "<<task_given.back()->type_of_work<<" che dura "
                <<task_given.back()->do_task_time<<" secondi\n\n";
            
                adopera(&task_given.back()->adding);

            }

            this_thread::sleep_for(chrono::seconds(task_given.back()->do_task_time));//simulo task da fare al worker

            //stampo il risultato della somma
            {
                lock_guard<mutex> lock(tasks_finished);
                task_given.back()->result=task_given.back()->finish_adding.get();

                cout<<"il lavoratore con ID "<<ID_worker<<" ha finito il lavoro "<<task_given.back()->type_of_work<<",esso era una somma e gli è durato "
                <<task_given.back()->do_task_time<<" secondi"<<" i numeri erano "<<task_given.back()->num1<<" e "<<task_given.back()->num2<<" abbiamo ottenuto "
                <<task_given.back()->result<<"\n\n";

                //funzione per controllare se ci sono altre task dentro la coda
                if(!get_new_work(*(&task_generated)))return;
                
            }
                       
        }

    }


    //funzione che permette a un worker di ottenere un lavoro disponibile al momento
    bool get_new_work(queue <shared_ptr<Task>>* task_generated){

        {
            lock_guard<mutex> lock(do_tasks);

            if(!(*task_generated).empty()){

                task_given.push_back((*task_generated).front());
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
            

        task_given.back()->num1 = distribution(generatore);//generare numero da 0 a 99
        task_given.back()->num2 = distribution(generatore);

        adding->set_value(task_given.back()->num1 + task_given.back()->num2);//mandare risultato a result nel mentre che aspetta

    }
    
    //-----------------------------------stampa in output tutte le task fatte dal singolo worker(funzione chiamata dal master)-----------------------------------//
    void show_results(){
        
        cout<<"il lavoratore con ID:"<<ID_worker<<" ha fatto le task\n";
        
        for(int i=0;i<task_given.size();i++){
            
            cout<<task_given.at(i)->type_of_work<<" e ha ottenuto "<<task_given.at(i)->result<<"\n";
            
        }

        cout<<"\n\n";
        
    }

};

class Master{

    public:

    vector <Worker> workers;
    queue <shared_ptr<Task>> task_generated;
    //-----------------------------------creazione tasks e workers-----------------------------------//
    void create_workers(){

        if(workers_doing_tasks>tasks_required){

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

            task_generated.push(make_shared<Task>((rand()%10)+1,type_task[i]));
            
        }

    }

    void name_tasks(){

        type_task.reserve(tasks_required);

        for(int i=0;i<tasks_required;i++){

            type_task.push_back("A"+to_string(i));

        }


    }
    //-----------------------------------terminazione thread-----------------------------------//
   void finish_threads(){

        for(auto& w : workers){

            w.control_thread();
        }

    }
    
    //-----------------------------------funzione che chiama ogni singolo worker per mostrare le task da ogniuno fatte-----------------------------------//
    void show_tasks(){
        
        for(auto& w : workers){
            
            w.show_results();
            
        }
        
    }

};

int main(){

    srand(time(0));

    Master M;//allocato nello stack

    //start del lavoro dei workers_doing_tasks
    M.name_tasks();
    M.create_tasks();
    M.create_workers();

    {
        unique_lock<mutex> lock(wait_ending_workers);

        continue_the_main.wait(lock,[]{return !workers_doing_tasks;});
    }
        
    cout<<"tutti i dipendenti hanno finito di lavorare\n\n";
    M.show_tasks();
    M.finish_threads();//i thread dentro worker vengono joinati

    return 0;
}
