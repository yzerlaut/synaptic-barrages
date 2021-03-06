#include "synaptic-barrages.h"
#include <iostream>
#include <main_window.h>
#include<fstream>
#include <stdlib.h>


extern "C" Plugin::Object*
createRTXIPlugin(void)
{
  return new SynapticBarrages();
}

static DefaultGUIModel::variable_t vars[] = {
    { "Vm", "Membrane potential (V)", DefaultGUIModel::INPUT, },
    { "Isyn (A)", "Output current (A)", DefaultGUIModel::OUTPUT, },
    { "nGe", "norm. exc conductance", DefaultGUIModel::STATE, },
    { "Gl (nS)", "Resting Input Conductance (nS)",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Qe/Gl", "norm. Exc. Synaptic Weight (nS)",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Te (ms)", "Exc. Synaptic Time constant (ms)",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Ee (mV)", "Exc. Reversal Potential (mV)",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Fe-min (Hz)", "Min Exc. Freq. of Poisson Process",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Fe-max (Hz)", "Max Exc. Freq. of Poisson Process",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Increments", "How many steps to take between min and max",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::UINTEGER, },
    { "Seed Number", "How many seeds for the same input level",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::UINTEGER, },
    { "Duration (ms)", "Duration of one stimulus", DefaultGUIModel::PARAMETER
      | DefaultGUIModel::DOUBLE, },
    { "Fixed Delay (ms)", "Fixed Time until step starts from beginning of cycle",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Random Delay (ms)", "Random Time until step starts from beginning of cycle",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "SEED", "Seed to generate the Poisson process stimulation",
      DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
};

static size_t num_vars = sizeof(vars) / sizeof(DefaultGUIModel::variable_t);

double SynapticBarrages::randZeroToOne()
{
  return rand() / (RAND_MAX + 1.);
}

SynapticBarrages::SynapticBarrages(void)
  : DefaultGUIModel("SynapticBarrages with Custom GUI", ::vars, ::num_vars)
{
  initParameters();
  setWhatsThis("<p><b>SynapticBarrages:</b><br>Transient barrages of excitatory and inhibitory shotnoise at various frequencies.</p>");
  DefaultGUIModel::createGUI(vars,
                             num_vars); // this is required to create the GUI
  update(INIT); // this is optional, you may place initialization code directly
                // into the constructor
  refresh();    // this is required to update the GUI with parameter and state
                // values
  QTimer::singleShot(0, this, SLOT(resizeMe()));
}


SynapticBarrages::~SynapticBarrages(void)
{
}

void
SynapticBarrages::execute(void)
{
  systime = count * period; // time in seconds

  if ((systime>Start_Vector[step_counter]) && !(stim_on)) {
    std::cout << systime <<  " " << Start_Vector[step_counter] << " " << step_counter << "\n";
    stim_on = true; // turn on step
    istim = 0;
  } 

  if ((systime>Stop_Vector[step_counter % Nmax_Episodes]) && (stim_on)) {
      stim_on = false; // turn off step
      step_counter++;
      step_counter = step_counter % Nmax_Episodes;
  }

  nGe = 0;
  if (stim_on) {
    nGe = Patterns[iFreq_Vector[step_counter]][Seed_Vector[step_counter]][istim % Length_max_pattern];
    istim++;
  }
  
  output(0) = (nGe*Gl*1e-9)*(Ee*1e-3-input(0));
  count++;
}

void
SynapticBarrages::initParameters(void)
{
  istim = 0;
  seed = 3 ;
  nGe = 0.0;
  nQe = 0.1;
  Te = 5.0; // ms
  Ee = 0.0; // mV
  Fe_min = 40.; Fe_max = 200.; // Hz
  Vm = -70.0 ;
  systime = 0.0;
  count = 0;
  period = RT::System::getInstance()->getPeriod() * 1e-9; // s
  Nfreq = 4 ;
  Nseed = 4 ;
  stim_on = false;
  step_counter = 0;
  delay=100;
  duration=200;
  random_delay = 500;
  output(0) = 0;
}

void
SynapticBarrages::LoadConductanceFromFile(void)
{
  std::string STRING;
  std::ifstream ConductanceFile;
  ConductanceFile.open("/home/yann/DATA/cell_conductance.txt");
  if (ConductanceFile.is_open()) {
    std::getline(ConductanceFile,STRING);
    Gl = std::stod(STRING);
  } else {
    std::cout << "--------------------------------";
    std::cout << " the conductance value was not loaded ";
    std::cout << "--------------------------------";
    Gl = 10.0;
  }
  ConductanceFile.close();
}

void
SynapticBarrages::initRandomization(void)
{
  // prepare a vector of frequencies for the random shuffling
  std::vector<int> freqvector;
  for (int i=0; i<Nfreq; ++i) freqvector.push_back(i);

  // random shuffling
  for (int i=0; i<Nmax_Episodes; ++i) {
    // we re-shuffle at every Nseed
    if ((i%Nseed)==0) std::random_shuffle( freqvector.begin(), freqvector.end() );
    iFreq_Vector[i] = freqvector[i%Nfreq];
    Seed_Vector[i] = (i/Nseed) % Nseed; // we explore the seed linearly
  }
  // now defining the temporal randomization
  Start_Vector[0] = 2*1e-3*(delay+random_delay) ; // second, 3 times delay to be sure to start later than recording
  Stop_Vector[0] = Start_Vector[0]+1e-3*duration ; // second
  for (int i=1;i<Nmax_Episodes;i++) {
    Start_Vector[i] = Stop_Vector[i-1]+1e-3*delay+
      1e-3*randZeroToOne()*random_delay; // second
    Stop_Vector[i] = Start_Vector[i]+1e-3*duration; // second
  }
  systime = 0.0;
  stim_on = false;
}

void
SynapticBarrages::set_filename(void)
{
  QSettings userprefs;
  userprefs.setPath(QSettings::NativeFormat, QSettings::SystemScope, getenv("HOME"));
  root_dir = userprefs.value("/dirs/data", getenv("HOME")).toString();
  QString data_dir = root_dir;
  time_t now=time(0);
  char day[12];
  strftime(day, 13, "/%Y_%m_%d/", localtime(&now));
  data_dir += day;
  if (QDir(data_dir).exists()==false) {
    /* insure that data file exists*/
    QDir().mkdir(data_dir); } 
  filename = data_dir;
  char hms[20];
  strftime(hms, 24, "%H:%M:%S.JSON", localtime(&now));
  filename += hms;
}

void
SynapticBarrages::storeRandomization(void)
{
  // we construct a text file that has the formatting of a JSON file (for python dictionary)
  SynapticBarrages::set_filename();
  std::ofstream myfile(filename.toLatin1().constData());
  if (myfile.is_open())
  {
    myfile << "{";
    // start episode vector
    myfile << "'start_vector':[";
    for (int i=0; i<Nmax_Episodes-1; ++i) myfile << Start_Vector[i] << ",";
    myfile << Start_Vector[Nmax_Episodes-1] << "],\n";
    // end episode vector
    myfile << "'stop_vector':[";
    for (int i=0; i<Nmax_Episodes-1; ++i) myfile << Stop_Vector[i] << ",";
    myfile << Stop_Vector[Nmax_Episodes-1] << "],\n";
    // frequency vector
    myfile << "'freq_vector':[";
    for (int i=0; i<Nmax_Episodes-1; ++i) myfile << iFreq_Vector[i] << ",";
    myfile << iFreq_Vector[Nmax_Episodes-1] << "],\n";
    // frequency vector
    myfile << "'seed_vector':[";
    for (int i=0; i<Nmax_Episodes-1; ++i) myfile << Seed_Vector[i] << ",";
    myfile << Seed_Vector[Nmax_Episodes-1] << "],\n";
    // 
    myfile << "'protocol_type':'synaptic_barrages'}";
    myfile.close();
  }
  else std::cout << "Unable to open file";
}

double SynapticBarrages::RdmNumber() {
  double rdm_number;
  /* (nastyly generated) uniformily distributed random number */  
  rdm_number = (rand() % 1000000) * 1e-6 ; 
  return rdm_number;
}

void
SynapticBarrages::fill_conductance_inputs(void)
{
  double freq = Fe_min; // starting frequency Fe_min
  for (int s=0; s<Nseed; ++s) {
    // loop over seeds
    freq = Fe_min;
    for (int f=0; f<Nfreq; ++f) {
      srand(seed+3*s+2); // reinitializing seeds
      // loop over frequencies
      freq += f*(Fe_max-Fe_min)/(Nfreq-1);
      Patterns[f][s][0] = 0;
      for (int i=0; i<Length_max_pattern-1; ++i) {
	// loop over the time axis
	Patterns[f][s][i+1] = Patterns[f][s][i]*exp(-period/(1e-3*Te)); 
	if (RdmNumber()<freq*period) Patterns[f][s][i+1] += nQe; // kept in nS here
      }
    }
  }
}



void
SynapticBarrages::update(DefaultGUIModel::update_flags_t flag)
{
  switch (flag) {
    case INIT:
      // functions called at the initialization
      LoadConductanceFromFile();
      setState("nGe", nGe);
      setParameter("Gl (nS)", QString::number(Gl)); 
      setParameter("Qe/Gl", QString::number(nQe)); 
      setParameter("Ee (mV)", QString::number(Ee));
      setParameter("Te (ms)", QString::number(Te)); 
      setParameter("Fe-min (Hz)", QString::number(Fe_min));
      setParameter("Fe-max (Hz)", QString::number(Fe_max));
      setParameter("Duration (ms)", duration);
      setParameter("Fixed Delay (ms)", delay);
      setParameter("Random Delay (ms)", random_delay);
      setParameter("Increments", Nfreq);
      setParameter("Seed Number", Nseed);
      setParameter("SEED", QString::number(seed));
      period = RT::System::getInstance()->getPeriod() * 1e-9; // s
      // Initialize counters
      step_counter = 0;
      // Randomize
      initRandomization();
      // Store the randomization on disk
      storeRandomization();
      // initialize the stimulation vector
      fill_conductance_inputs();
      nGe = 0; // normalized excitatory conductance
      output(0) = 0;
      break;

    case MODIFY:
      Gl = getParameter("Gl (nS)").toDouble();
      nQe = getParameter("Qe/Gl").toDouble();
      Ee = getParameter("Ee (mV)").toDouble();
      Te = getParameter("Te (ms)").toDouble(); 
      Fe_min = getParameter("Fe-min (Hz)").toDouble(); 
      Fe_max = getParameter("Fe-max (Hz)").toDouble();
      duration = getParameter("Duration (ms)").toDouble();
      delay = getParameter("Fixed Delay (ms)").toDouble();
      random_delay = getParameter("Random Delay (ms)").toDouble();
      Nfreq = getParameter("Increments").toInt();
      Nseed = getParameter("Seed Number").toInt();
      seed = getParameter("SEED").toInt(); 
      // Initialize counters
      step_counter = 0;
      // Randomize
      initRandomization();
      // Store the randomization on disk
      storeRandomization();
      // initialize the stimulation vector
      fill_conductance_inputs();
      nGe = 0; 
      output(0) = 0;
      break;

    case UNPAUSE:
      systime = 0.0;
      break;

    case PAUSE:
      systime = 0.0;
      stim_on = false;
      output(0) = 0;
      nGe = 0;
      break;

    case PERIOD:
      period = RT::System::getInstance()->getPeriod() * 1e-9; // ms
      break;

    default:
      break;
  }
}
