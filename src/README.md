In tema mea, am simulat protocolul BitTorrent de partajare peer-to-peer de fisiere, folosind MPI. Pornind de la schelet,
in functie de rank-ul procesului, 0 - tracker sau client, s-au apelat functii diferite. Pentru tracker, s-a apelat
functia de tracker, care primeste toate fisierele de la clienti si le stocheaza in variabila sa files, neacceptand 
dubluri. In acelasi timp, fiecare client citeste din fisierul sau toate fisierele si segmentele pe care le are, respectiv
fisierele pe care le doreste. Cand tracker-ul a terminat de primit tot, trimite un mesaj "ACK" la clienti, pentru ca acestia
sa isi poate incepe treaba de download/upload. 

In functia de download, fiecare client ia la rand fisierele pe care le doreste si creeaza un file text pentru ele, in care 
va printa hash-urile. Trimie un TAG_REQUEST catre tracker cu numele fisierului pentru a primi swarm-ul acestuia, urmand 
sa stie ulterior cine este seed/peer si de ce segmente are nevoie din acest fisier. Dupa ce primeste, trece prin fiecare segment
si cauta cel mai putin ocupat client de la care sa descarce.
Pentru a face asta, intreaba fiecare seed/peer daca are hash-ul respectiv si, daca au, le cere variabila busy, pentru a sti
de cate descarcari se ocupa deja. La final, verifica cel mai putin ocupat client si ii cere acestuia hash-ul, asteptand 
raspuns "ACK" pentru a simula descarcarea hash-ului, si marcheaza segmentul ca descarcat. Cand termina de trecut prin toate
fisierele, ii trimite un mesaj tracker-ului cu TAG_CLIENT_DONE pentru ca acesta sa stie ca acest client si-a finalizat descarcarea.
In functia upload, clientul asteapta mereu mesaje si le separa in functie de continut. Daca primeste mesaj de busy, trimite 
variabila la sursa. Daca primeste mesajul "ai hash?", primeste si hash-ul in cauza si il cauta in tot ce are descarcat, 
raspunzand cu "ACK" daca il are, si cu "NACK" in caz contrar. Daca primeste mesaj cu un hash, stie ca este pentru download, 
cauta hash-ul si trimite ACK inapoi, pentru a simula descarcarea. Totodata, in acest caz, ii creste si variabila busy, 
ocupandu-se acum de inca un download. Daca primeste mesaj "ACK", stie ca este pentru a termina functia upload, deci face break.
De asemenea, functia de download are si o variabila ten_counter, care tine cont de numarul segmente pe care le-a descarcat din
ceea ce isi doreste fiecare client, astfel incat, la fiecare 10 descarcari, sa isi actualizeze seeds/peers ai fisierului, printr-un
request catre tracker.

Tracker-ul are o variabila prin care tine cont de numarul de clienti care si-au terminat treaba. Pana cand nu s-au terminat, 
tracker-ul primeste in continuu mesaje, clasificandu-le in functie de TAG. Daca tag-ul este TAG_REQUEST, trimite swarm-ul 
fisierului al carui nume se afla in mesaj. Daca primeste TAG_PEERS, trimite vectorul de seeds/peers pentru acest fisier, pentru
a se actualiza. Daca primeste TAG_CLIENT_DONE, incrementeaza variabila pentru nr clienti care au terminat. La final, cand au 
terminat toti clienti, tracker-ul le trimite cate un mesaj de ACK, pentru ca acestia sa isi finalizeze si functiile de upload.