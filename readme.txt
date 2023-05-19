Paduretu David-Pavel 322CC
Pt aceasta tema facut cerere pt 2 sleep days

ATENTIE: NU VERIFICATI SI TESTUL 'quick-flow'

TEST.H:
   In acest fisier am definit structurile ce urmeaza sa fie utilizate, respectiv:
       - structura pachetului TCP, fiindu-mi necesara la afisarea mesajului 
pentru un client TCP pentru a avea formatul corespunzator, ce contine:
ip-ul si portul clientului UDP, topicul, tipul de date din payload si payload-ul.
       - structura topicului, ce contine numele, sf-ul (pentru a stabili modul
de trimitere al mesajelor, in functie de cum sunt abonate la topic) si un 
vector de mesaje in care se vor retine mesajele ce au fost trimise cat timp
un subscriber a fost deconectat, cu conditia ca sf ul la topicul respectiv sa
fie 1
      - structura subscriberului, ce contine date despre subscriber si topicurile
la care este abonat

SUBSCRIBER.CPP:
   In cadrul subscriberului, se verifica daca se citeste de la tastatura, caz in 
care se pot primi doar comenzile "exit", "subscribe" si "unsubscribe", urmand ca 
pentru exit sa se inchida subscriberul si sa se deconecteze de la server, iar pentru 
celelalte doua sa se afiseze un mesaj corespunzator si sa anunte serverul daca se
aboneaza/dezaboneaza la un topic. 
    Mesajul trimis catre server este in format text, de exact 100 bytes, terminat cu \0.
    In cazul in care se primeste de la server, se va afisa mesajul de pe
socket, care a fost in mare parsat.

SERVER.CPP:
     In cazul in care se primeste de la tastatura comanda "exit" se inchide 
atat serverul cat si toti clientii conectati la acesta (se inchid socketii ce
comunica cu userii, iar acest lucru ii va determina sa isi incheie executia). 
    In cazul in care ma aflu pe socketul TCP verific in functie de id ul primit daca
exista in lista de subscriberi si daca este conectat, deoarece in cazul in care
el deja exista in lista, dar nu este conectat, i se va modifica doar campul "connect"
(devine 1).
      De asemenea, pentru clientii carora li s-au trimis mesaje atunci cand au fost 
deconectati, conform politicii store-and-forward, li se vor afisa mesajele stranse.
      Atunci cand clientul nu a fost gasit, se va crea un nou subscriber
(completandu-i-se campurile id, socket si connect) ce se va adauga in lista.
     In cazul in care sunt pe socketul UDP, inseamna ca se primesc mesajele de la
clientii UDP si vor trebui parsate.
      Prelucrarea payload-ului se va face predominant in server, inclusiv parsarea
      datelor efective, astfel incat clientul sa aiba foarte putin de lucru (doar
      de separat parametrii cu "-" si de afisat in ordine).
    Pentru ca serverul trebuie sa asigure trimiterea mesajului catre toti clientii
care s-au abonat la topicul respectiv, intalnim din nou cele doua cazuri de store-and-
forward:
      -> daca clientul este abonat la topic (lucru pe care-l verific prin parcurgerea
tuturor abonatilor si a topicurilor sale) si este conectat i se va trimite mesajul
       -> daca clientul este abonat la topic, nu este conectat, dar are setat sf-ul pe 1,
in vectorul de mesaje(ce se trimit cat el este deconectat de la topicurile la care este
abonat corespunzator) se va adauga si acest mesaj
       Atunci cand pe unul din socketii de client s-au primit date inseamna ca a avut
loc abonarea/dezabonarea la un topic. Abonarea propriu-zisa consta in verificarea
ca primul cuvant din buffer este "subscribe", apoi se va parcurge restul bufferului
pentru a extrage numele topicului si sf-ul. In campul ce contine topicurile la care
este abonat clientul, se va adauga si acest topic.
