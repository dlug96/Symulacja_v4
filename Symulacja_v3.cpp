// Symulacja_v3.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include "Header.h"

#include "process.h"
#include "event.h"
#include "AgendaList.h"

#include "patient.h"
#include "donator.h"
#include "bloodCentre.h"
#include "bloodSample.h"
#include "bloodStorage.h"
#include "delivery.h"
#include "research.h"

#include "generator_01.h"
#include "generator_Exponential.h"
#include "generator_Gauss.h"
#include "generator_Bernoulli.h"
#include "generator_Geometrical.h"
#include "generator_5_10.h"

#include "seeds.h"

using namespace std;

//---------------------------------Klasa do zapisu do pliku-------------------------------//
class saveToFile
{
public:
	saveToFile(double x, double y) : m_x(x), m_y(y) {}

	friend istream& operator >> (istream& in, saveToFile& obj);
	friend ostream& operator << (ostream& out, const saveToFile& obj);

private:
	double m_x;
	double m_y;
};

istream& operator >> (istream& in, saveToFile& obj)
{
	in >> obj.m_x;
	in >> obj.m_y;
	return in;
}

ostream& operator << (ostream& out, const saveToFile& obj)
{
	out << obj.m_x << ' ';
	out << obj.m_y << endl;
	return out;
}
//Grupy krwi
//A - true
//B - false
//Dostawy
//Awaryjna - true
//Normalna - false



//---------------------------------------------------------------------------------------//
int main()
{
	//Wybór trybu pracy
	bool stepMode;
	string workMode;
	while (true)
	{
		cout << "Czy uruchomic tryb krokowy? (T/N)" << endl;
		getline(cin, workMode);
		if (workMode == "t" || workMode == "T") {
			stepMode = true;
			break;
		}
		else if (workMode == "n" || workMode == "N") {
			stepMode = false;
			break;
		}
		else continue;
	}
	

	//-------------------------------------------------------------------------------------------------//
	//Podanie danych

		//Stałe
	int R = 10;		//Poziom jednostek krwi dla wezwania dostawy normalnej
	int N = 30;		//Liczba jednostek krwi w dostawie normalnej
	double Z = 1900.0;	//Średnia rozkładu dla zamówienia normalnego
	double T1 = 300.0;	//Czas utylizacji T1
	double T2 = 500.0;	//Czas utylizacji T2
	double P = 200.0;	//Średnia rozkładu dla pojawiania się kolejnych pacjentów
	double W = 0.23;	//Odwrotność średniej rozkładu jednostek krwi dla pacjenta
	double E = 1200.0;	//Średnia i wariancja rozkładu dla zamówienia awaryjnego
	double EW = 0.1;
	int Q = 60;			//Liczba jednostek krwi w zamówieniu awaryjnym
	double L = 850.0;	//Średnia rozkładu dla zgłaszania się dawców
	double A = 0.07;	//Poziom prawdopodobieństwa dostawy awaryjnej

		//Zegar systemowy i wskaźnik do niego
	double clock = 0.0;
	double *clock_adr = &clock;
	AgendaList *Agenda = new AgendaList(nullptr);

		//Ziarna generatorów
	double generatorSeed = 100.0;


	//-------------------------------------------------------------------------------------------------//
	//Tworzenie obiektów symulacji

		//Centrum krwiodawstwa i magazyn
	bloodCentre *Centre = new bloodCentre(seeds[0][0], Z, 
		seeds[0][1], E, EW,
		seeds[0][2], P,
		seeds[0][3], L,
		seeds[0][4], W,
		seeds[0][5],
		seeds[0][6],
		seeds[0][7],
		clock_adr, Agenda, clock_adr);
	bloodStorage *storage = Centre->storage;

		//Zainicjowanie pierwszego pacjenta i pierwszego dawcy
	patient *sourcePatient = new patient(Centre->generatorForBloodSamplesNeeded->generate_Geometrical(), Centre, Agenda, clock_adr);
	donator *sourceDonator = new donator(Centre, Agenda, clock_adr);
	sourcePatient->Activate(0.0);
	sourceDonator->Activate(Centre->generatorForDonators->generate_Exponential());

	double tillEnd = 15000.0;

	process *current = nullptr;

	int maxPatients = 200;

	double stats[101];

	int lastIndex = -1;

	bool zeroStats = false;

	

	//----------------------------------Pętla główna-------------------------------------//
	while (Centre->patients < maxPatients)
	{
		//Zdejmij zdarzenie z listy
		current = Agenda->returnfirst()->proc;
		clock = Agenda->returnfirst()->eventTime;
		Agenda->removeFirst();
		current->Execute();
		
		//Przejście pacjenta do kolejki
		if (Centre->transfusionPoint == nullptr) {
			if (Centre->queueOfPatients.empty() != true) {
				Centre->transfusionPoint = Centre->queueOfPatients.front();
				Centre->queueOfPatients.pop();
				cout << "Pacjent opuscil kolejke" << endl;
			}
		}

		//Czy wysłać dostawę normalną

		//Sprawdź czy trzeba wyslać dostawę normalną
		//Grupa A
		if (storage->countSamples(true) < R) {
			if (storage->normalFlagA == false) {
				storage->normalFlagA = true;
				delivery *sendNormalDelivery = new delivery(false, true, storage, clock_adr, Agenda);
				sendNormalDelivery->Activate(Centre->generatorForNormalTransport->generate_Exponential());
				sendNormalDelivery = nullptr;
				delete sendNormalDelivery;
				Centre->nordeliveries += 1;
				cout << "Wyslano dostawe zwykla na grupe krwi A" << endl;
			}
		}
		//Grupa B
		if (storage->countSamples(false) < R)
		{
			if (storage->normalFlagB == false)
			{
				storage->normalFlagB = true;
				delivery *sendNormalnDelivery = new delivery(false, false, storage, clock_adr, Agenda);
				sendNormalnDelivery->Activate(Centre->generatorForNormalTransport->generate_Exponential());
				sendNormalnDelivery = nullptr;
				delete sendNormalnDelivery;
				Centre->nordeliveries += 1;
				cout << "Wyslano dostawe zwykla na grupe krwi B" << endl;
			}
		}
		
		//Transfuzja

		//Starcza jednostek krwi
		if (Centre->transfusionPoint != nullptr) {
			if (storage->countSamples(Centre->transfusionPoint->bloodGroup) >= Centre->transfusionPoint->bloodNeeded) {
				//Grupa krwi A
				if (Centre->transfusionPoint->bloodGroup) {
					for (int i = 0; i < Centre->transfusionPoint->bloodNeeded; i++) {
						storage->listOfSamplesGroupA->next->myEvent->unscheduleMe();
						storage->listOfSamplesGroupA->next->removeFromList();
					}
					cout << "Pacjent pobral " << Centre->transfusionPoint->bloodNeeded << " jednostek krwi grupy A" << endl;
				}
				//Grupa krwi B
				else {
					for (int i = 0; i < Centre->transfusionPoint->bloodNeeded; i++) {
						storage->listOfSamplesGroupB->next->myEvent->unscheduleMe();
						storage->listOfSamplesGroupB->next->removeFromList();
					}
					cout << "Pacjent pobral " << Centre->transfusionPoint->bloodNeeded << " jednostek krwi grupy B" << endl;
				}

				
				//Usunięcie pacjenta ze stanowiska
				delete Centre->transfusionPoint;
				Centre->transfusionPoint = nullptr;
				cout << "Pacjent zakonczyl leczenie" << endl;

				//Nowy pacjent wychodzi z kolejki
				if (Centre->queueOfPatients.empty() != true) {
					Centre->transfusionPoint = Centre->queueOfPatients.front();
					Centre->queueOfPatients.pop();
					cout << "Pacjent opuscil kolejke" << endl;
				}
				else cout << "Kolejka pusta" << endl;
			}

			//Nie starcza jednostek krwi - wyślij dostawę awaryjną
			else if (storage->countSamples(Centre->transfusionPoint->bloodGroup) < Centre->transfusionPoint->bloodNeeded)
			{
				//Jeśli pacjent ma grupę A
				if (Centre->transfusionPoint->bloodGroup) {
					if (storage->urgentFlagA == false) {
						storage->urgentFlagA = true;
						delivery *sendUrgentDelivery = new delivery(true, Centre->transfusionPoint->bloodGroup, storage, clock_adr, Agenda);
						sendUrgentDelivery->Activate(Centre->generatorForUrgentTransport->generate_Gauss());
						//sendUrgentDelivery->Activate(5.0);
						sendUrgentDelivery = nullptr;
						delete sendUrgentDelivery;
						Centre->urgdeliveries += 1;
						cout << "Wyslano dostawe awaryjna na grupe krwi A" << endl;
					}
				}
				//Jeśli pacjent ma grupę B
				else {
					if (storage->urgentFlagB == false) {
						storage->urgentFlagB = true;
						delivery *sendUrgentDelivery = new delivery(true, Centre->transfusionPoint->bloodGroup, storage, clock_adr, Agenda);
						sendUrgentDelivery->Activate(Centre->generatorForUrgentTransport->generate_Gauss());
						//sendUrgentDelivery->Activate(5.0);
						sendUrgentDelivery = nullptr;
						delete sendUrgentDelivery;
						Centre->urgdeliveries += 1;
						cout << "Wyslano dostawe awaryjna na grupe krwi B" << endl;
					}
				}
				//Jeśli zostały jakieś jednostki krwi - pacjent bierze wszystko
				if (storage->countSamples(Centre->transfusionPoint->bloodGroup) > 0) {
					Centre->transfusionPoint->bloodNeeded -= storage->countSamples(Centre->transfusionPoint->bloodGroup);
					int takeAllBlood = storage->countSamples(Centre->transfusionPoint->bloodGroup);

					if (Centre->transfusionPoint->bloodGroup) {
						for (int i = 0; i < takeAllBlood; i++) {
							storage->listOfSamplesGroupA->next->myEvent->unscheduleMe();
							storage->listOfSamplesGroupA->next->removeFromList();
						}
						cout << "Pacjent pobral " << takeAllBlood << " jednostek krwi grupy A" << endl;
					}
					else {
						for (int i = 0; i < takeAllBlood; i++) {
							storage->listOfSamplesGroupB->next->myEvent->unscheduleMe();
							storage->listOfSamplesGroupB->next->removeFromList();
						}
						cout << "Pacjent pobral " << takeAllBlood << " jednostek krwi grupy B" << endl;
					}
				}
			}
		}
		


		//Badania naukowe
			//Czy wysłać zgłoszenie
		if (storage->countSamples(true) > 30) {
			if (Centre->researchFlagA == nullptr) {
				Centre->researchFlagA = new research(Centre, true, Agenda, clock_adr);
				Centre->researchFlagA->Activate(300.0);
				cout << "Zaplanowano pobranie probek krwi grupy A na badania" << endl;
			}
		}
		if (storage->countSamples(false) > 30) {
			if (Centre->researchFlagB == nullptr) {
				Centre->researchFlagB = new research(Centre, false, Agenda, clock_adr);
				Centre->researchFlagB->Activate(300.0);
				cout << "Zaplanowano pobranie probek krwi grupy B na badania" << endl;
			}
		}

			//Czy odwołać zgłoszenie
		if (storage->countSamples(true) <= 30) {
			if (Centre->researchFlagA != nullptr) {
				Centre->researchFlagA->myEvent->unscheduleMe();
				delete Centre->researchFlagA;
				Centre->researchFlagA = nullptr;
				cout << "Odwolano pobranie probek krwi grupy A na badania" << endl;
			}
		}
		if (storage->countSamples(false) <= 30) {
			if (Centre->researchFlagB != nullptr) {
				Centre->researchFlagB->myEvent->unscheduleMe();
				delete Centre->researchFlagB;
				Centre->researchFlagB = nullptr;
				cout << "Odwolano pobranie probek krwi grupy B na badania" << endl;
			}
		}

		//Zakończenie fazy początkowej
		
		if (Centre->patients == 150 && zeroStats == false) {
		Centre->urgdeliveries = 0;
		Centre->nordeliveries = 0;
		Centre->patients = 0;
		Centre->donators = 0;
		zeroStats = true;
		maxPatients = 100;
		cout << "Zakonczenie fazy poczatkwej" << endl;
		//system("pause");
		}

		if (zeroStats) {
			if (lastIndex != Centre->patients) {
				lastIndex = Centre->patients;
				stats[lastIndex] = ((double)Centre->urgdeliveries) / ((double)Centre->urgdeliveries + (double)Centre->nordeliveries);
				if (stats[lastIndex] < 0)stats[lastIndex] = 0;
			}

			cout << "Zegar systemowy: " << clock << endl;
			cout << "Jednostki krwi grupy A w magazynie: " << storage->countSamples(true) << endl;
			cout << "Jednostki krwi grupy B w magazynie: " << storage->countSamples(false) << endl;
			cout << "Pacjenci z grupa A w kolejce: " << Centre->queueOfPatients.size() << endl;
			cout << "Zdarzen na liscie: " << Agenda->checkIfOK() << endl;
			cout << "Pacjenci: " << Centre->patients << endl;
			cout << "Dawcy: " << Centre->donators << endl;
			cout << "Badania: " << Centre->researches << endl;
			cout << "Zamowienia awaryjne: " << Centre->urgdeliveries << endl;
			cout << "Zamowienia zwykle: " << Centre->nordeliveries << endl;
			cout << "Procent zamowien awaryjnych: " << ((double)Centre->urgdeliveries) / ((double)Centre->urgdeliveries + (double)Centre->nordeliveries) << endl;
			cout << endl;


			if (stepMode)system("pause");
		}
	}

	

	ofstream outFile("fazaPoczatkowa.dat");
	for (int i = 0; i < 100 ; i++) {
		saveToFile myObj(i, stats[i]);
		outFile << myObj;
		cout << i << ": " << stats[i] << endl;
	}
	outFile.close();

	system("pause");
	return 0;
}

