#ifndef __TIMER_H__
#define __TIMER_H__

namespace sirius 
{

class Timer
{
    private:
        
        /// string label of the timer
        std::string label_;
        
        /// starting time
        timeval starting_time_;

        /// true if timer is running
        bool active_;

        /// mapping between timer name and timer's value
        static std::map<std::string, double> timers_;
        
        /// number of measures
        static std::map<std::string, int> tcount_;
    
    public:
        
        Timer(const std::string& label__, bool start__ = true) : label_(label__), active_(false)
        {
            if (timers_.count(label_) == 0) 
            {
                timers_[label_] = 0;
                tcount_[label_] = 0;
            }

            if (start__) start();
        }

        ~Timer()
        {
            if (active_) stop();
        }

        void start()
        {
            if (active_)
                error(__FILE__, __LINE__, "timer is already running");

            gettimeofday(&starting_time_, NULL);
            active_ = true;
        }

        void stop()
        {
            if (!active_)
                error(__FILE__, __LINE__, "timer was not running");

            timeval end;
            gettimeofday(&end, NULL);
            timers_[label_] += double(end.tv_sec - starting_time_.tv_sec) + 
                               double(end.tv_usec - starting_time_.tv_usec) / 1e6;
            tcount_[label_]++;

            active_ = false;
        }

        static void print()
        {
            std::map<std::string, double>::iterator it;
            for (it = timers_.begin(); it != timers_.end(); it++)
                printf("%-60s : %10.4f (total)   %10.4f (average)\n", it->first.c_str(), it->second, 
                                                                      it->second/tcount_[it->first]);
        }
 
        //static void print(std::string tname);
};

std::map<std::string,double> Timer::timers_;
std::map<std::string,int> Timer::tcount_;
std::map<std::string,Timer*> ftimers;

};

#endif // __TIMER_H__