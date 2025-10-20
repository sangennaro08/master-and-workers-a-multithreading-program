#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore>
#include <future>
#include <cstdlib>
#include <random>

using namespace std;

mutex mut;
mutex dip;

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
    task* t;
    thread th;
    promise<double> final;
    mt19937 generatore;

    worker(int ID,vector <task*>& task_generated):
    generatore(chrono::system_clock::now().time_since_epoch().count() + ID)

    {

        ID_worker=ID;

        {
            lock_guard<mutex> lock(dip);
            t=task_generated.at(task_generated.size()-1);
            
            task_generated.pop_back();
            
            th=thread(&worker::start_work,this,&task_generated);//evidenzia in base all'oggetto l'indirizzo della funzione
            
        }

    }

    
    ~worker(){delete t;}

    void start_work(vector <task*>* task_generated){

        while(true){

            {
                
                lock_guard<mutex> lock(mut);
                cout<<"il lavoratore "<<ID_worker<<" sta iniziando la task di tipo "<<t->tipo<<" che dura "
                <<t->tempo_necessario<<" secondi\n\n";
            
                thread ta(&worker::adopera,this,ref(t->adding));
                ta.join();

            }

            this_thread::sleep_for(chrono::seconds(t->tempo_necessario));//simulo task da fare al worker

            {
                lock_guard<mutex> lock(mut);
                double f=t->finish_adding.get();

                cout<<"il thread con ID "<<ID_worker<<" ha finito il lavoro "<<t->tipo<<",esso era una somma e gli è durato "
                <<t->tempo_necessario<<" secondi"<<" i numeri erano "<<t->num1<<" e "<<t->num2<<" abbiamo ottenuto "<<f<<"\n\n";

                if(!get_new_work(&task_generated))return;

            }   
            
            

        }

    }


    //funzione che permette a un worker di ottenere un lavoro disponibile al momento
    bool get_new_work(vector <task*>** task_generated){

        {
            lock_guard<mutex> lock(dip);

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

    //controlla se il thread è terminabile
    void control_thread(){

        if(th.joinable()){

            th.join();

        }

    }

    //somma dei numeri
    void adopera(promise <double>& adding){
        
        {
            lock_guard<mutex> lock(dip);

            uniform_int_distribution<int> distribution(0, 99);
            

            t->num1 = distribution(generatore);
            t->num2 = distribution(generatore);
            adding.set_value(t->num1+t->num2); 
        }

    }

};

class master{

    public:

    vector <worker*> workers;
    vector <task*>   task_generated;

    void create_workers(){

        for(int i=0;i<dipendenti;i++){

            promise<double> obtain_ris;

            workers.push_back(new worker(i,task_generated));

        }

    }

    void create_tasks(){

        for(int i=0;i<lavori;i++){

            task_generated.push_back(new task((rand()%10)+1,type_task[i]));
            

        }

    }

    void delete_workers(){

        for(auto w : workers){

            w->control_thread();
            delete w;
        }

    }

    void delete_tasks(){

        for(auto t : task_generated){

            delete t;

        }

    }

};

int main(){

    srand(time(0));

    master M;

    //start del lavoro dei dipendenti
    M.create_tasks();
    M.create_workers();

    while(dipendenti!=0){


    }

    cout<<"tutti i dipendenti hanno finito di lavorare\n";
    
    //libero memoria allocata
    M.delete_workers();
    M.delete_tasks();

    return 0;
}