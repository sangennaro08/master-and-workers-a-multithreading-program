#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <memory>
#include <condition_variable>
#include <atomic>
#include <chrono>

using namespace std;
 
class task;
//variabili globali
mutex task_finished;
mutex do_tasks;
mutex wait_ending_workers;

condition_variable continue_in_main;

atomic<int> tasks_to_do=20;
atomic<int> workers_doing_tasks=5;

vector<string> type_task;

//struct contenente le informazioni di ogni task
struct Task{
    
    public:
    size_t do_task_time;
    string type_of_work;

    Task(size_t t,string type):
    do_task_time(t),
    type_of_work(type){}
};

//classe worker lavora una task a lui data e poi altre SE rimaste
class Worker{

    thread th;
    shared_ptr<Task> task_given;
    int ID_worker;

    public:
    //-----------------------------------costruttore che crea il thread di worker-----------------------------------//      
    Worker(int ID,queue <shared_ptr<Task>>& task_generated):ID_worker(ID){

        {
            if(!task_generated.empty())
            {

            lock_guard<mutex> lock(do_tasks);

            task_given=task_generated.front();
            task_generated.pop();           
            th=thread(&Worker::work_task,this,&task_generated);//errore per refence dandling meglio farlo con puntatore come fatto qua
            }
        }               
    }
    //-----------------------------------funzione che simula il lavoro delle task-----------------------------------//
    void work_task(queue <shared_ptr<Task>>* task_generated){
        
        while(true)
        {
            {
                lock_guard<mutex> lock(task_finished);
                cout<<"il lavoratore "<<ID_worker<<" sta iniziando la task di tipo "<<task_given->type_of_work<<" che dura "
                <<task_given->do_task_time<<" secondi\n\n";
            }
    
            this_thread::sleep_for(chrono::seconds(task_given->do_task_time));//simulo task da fare al worker
       
            {
                lock_guard<mutex> lock(task_finished);
                    
                cout<<"il lavoratore "<<ID_worker<<" ha completato la task di tipo "<<task_given->type_of_work<<" che è durato "
                <<task_given->do_task_time<<" secondi\n\n";

                if(!get_new_task(task_generated))return;
            }            
        }        
    }

    //-----------------------------------terminazione thread chiamato dal master-----------------------------------//
   void join_thread() {
    if (th.joinable())th.join();
    }

    //-----------------------------------worker prende una task se presente nella queue----------------------------//
    bool get_new_task(queue <shared_ptr<Task>>* task_generated){

        //se il thread vede che ci sono altre tasks prende quella per ultima inserita e viene tolta(tolto dall'utilizzo ma non dinamicamente)
        {

        lock_guard<mutex> lock(do_tasks);

        if(!task_generated->empty())
        { 
            task_given=task_generated->front();
            cout<<"il lavoratore "<<ID_worker<<" ha deciso di prendere il lavoro di tipo "<<task_generated->front()->type_of_work<<"\n\n";
            task_generated->pop();
            
            return true;           
        }//viene definito quale task è stata presa da ogni worker se presente

        workers_doing_tasks--;
        if(!workers_doing_tasks)continue_in_main.notify_one();           
        return false;
        }           
    }   
};

//ha sotto controllo i lavoratori che li genera assieme alle task(ha come compito anche quello di congedare i dipendenti)
class Master{
    
    vector <Worker> tot_dipendenti;
    queue  <shared_ptr<Task>> task_generated;

    public:

    //-----------------------------------creazioni tasks e workers-----------------------------------//
    void create_tasks(){
        
        for(int i=0;i<tasks_to_do;i++)
        {
            task_generated.push(make_shared<Task>((rand()%10)+1,type_task[i]));           
        }      
    }
    
    void create_workers(){

        if(workers_doing_tasks>tasks_to_do)
        {
            int diminuire=workers_doing_tasks-tasks_to_do;
            workers_doing_tasks-=diminuire;

            cout<<"dipendenti diminuiti a "<<workers_doing_tasks<<" in quanto sono stati inseriti di più di quanto necessari\n\n";          
        }

        tot_dipendenti.reserve(workers_doing_tasks);

        for(int i=0;i<workers_doing_tasks;i++)
        {      
            tot_dipendenti.emplace_back(i, task_generated);//reso tutte le variabili di worker visto che non sono utlizzate dall'esterno
        }       
    }

    void name_tasks(){

        type_task.reserve(tasks_to_do);

        for(int i=0;i<tasks_to_do;i++)
        {
            type_task.push_back("A"+to_string(i));
        }
    }
    //-----------------------------------terminazione thread-----------------------------------//
    void finish_threads(){

        for(auto& w : tot_dipendenti)
        {
            w.join_thread();
        }
    }  
};

int main()
{
    srand(time(nullptr));
    
    Master M;
    
    //inizializzazione dipendenti e lavori
    M.name_tasks();
    M.create_tasks();
    M.create_workers();
    
    {
        unique_lock<mutex> lock(wait_ending_workers);               //condition variable per far sprecare meno uso della CPU
        continue_in_main.wait(lock,[]{return !workers_doing_tasks;});
    }
        
    cout<<"tutti i lavoratori hanno finito di lavorare\n";
    M.finish_threads();

    return 0;
}
