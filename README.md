# Mysterium-Node-Monitor-API-from-RPI
Monitoring Myst earnings on an ESP32-CYD using data fetched via API from a Raspberry Pi.
<img width="2160" height="3840" alt="IMG-20260927-WA0001" src="https://github.com/user-attachments/assets/eaf62c48-1a39-4e56-a2aa-61a611f845ce" />
<img width="504" height="468" alt="Myst1" src="https://github.com/user-attachments/assets/13c16e44-63b3-454a-9cba-ca505a70c255" />
<img width="506" height="592" alt="Myst2" src="https://github.com/user-attachments/assets/1fc42939-22b8-4a2a-990c-8959eb51cdd8" />
<img width="508" height="510" alt="Mist4" src="https://github.com/user-attachments/assets/23809e30-6073-4fca-90a5-c3cb724a300c" />
<img width="507" height="749" alt="Myst3" src="https://github.com/user-attachments/assets/0cb80fd6-d67c-43b5-b174-46079ae2cf1c" />

# Mysterium Node Monitor pe ESP32

Am facut un mic ecran dedicat pentru nodul meu Mysterium, ca sa nu mai stau sa intru mereu pe 【entity-Raspberry Pi¦canonical_name=Raspberry Pi】 sa verific castigurile.

Ruleaza pe un ESP32 cu ecran - ala galben ieftin CYD (ESP32-2432S028).

Ce face de fapt?

Se conecteaza la nodul tau Mysterium din casa si iti arata live pe ecran:

- Cat ai strans in total - Total Settled in MYST
- Cat ai neincasat inca - Unsettled Earnings
- Balanta curenta
- Versiunea nodului si daca nodul are calitate buna (GREAT / POOR)
- Ora exacta

Si cel mai fain - isi regleaza singur lumina. Are senzor de lumina si daca e intuneric in camera scade luminozitatea, daca e zi o da la maxim. Nu te orbeste noaptea.

Cum arata?

Pe ecranul mic ai 2 carduri mari, usor de citit:
1. Sus cu galben - totalul strans
2. La mijloc cu albastru - ce ai in asteptare
3. Jos o bara cu 3 chestii: ora, versiunea si o bulina verde/rosie pentru calitate.

Cand il pornesti iti apare logo-ul Mysterium 5 secunde si apoi intra in dashboard.

Daca atingi ecranul face refresh instant. Daca apesi pe i din colt intri in pagina de System Info unde vezi IP-ul, semnalul Wi-Fi, temperatura procesorului, etc.

Partea cea mai utila - pagina Web

Nu trebuie sa stai langa el. ESP-ul face si o pagina web care arata IDENTIC cu ecranul.

Intri de pe telefon sau laptop pe http://ip-ul_esp-ului si vezi in timp real castigurile, de oriunde din casa. Pagina se actualizeaza singura la fiecare secunda.

Ai si pagina /info cu toate detaliile nodului.

Practic e un monitor permanent pentru nod. Il pui pe birou langa router si stii tot timpul daca nodul tau merge, cat produce si daca are probleme de calitate.

Foarte util daca ai mai multe noduri sau daca nu vrei sa tii un monitor mare aprins doar pentru asta.

De ce ai nevoie?

Un modul CYD de ~80 lei si un Raspberry Pi unde ruleaza deja nodul Mysterium. Atat.

Se configureaza o singura data cu numele Wi-Fi-ului si IP-ul de la Pi si gata. Uita de el.


# Mysterium Node Monitor on ESP32

I made a small dedicated screen for my Mysterium node, so I don't have to log into my Raspberry Pi all the time to check my earnings.

It runs on an ESP32 with a display - the cheap yellow CYD (ESP32-2432S028).

What does it actually do?

It connects to your Mysterium node at home and shows you live on the screen:

- How much you earned in total - Total Settled in MYST
- How much is still pending - Unsettled Earnings
- Current Balance
- Node Version and if the node has good quality (GREAT / POOR)
- Exact Time

And the coolest part - it adjusts its own brightness. It has a light sensor and if it's dark in the room it lowers the brightness, if it's daytime it goes to max. It won't blind you at night.

How does it look?

On the small screen you have 2 big cards, easy to read:
1. On top in yellow - the total collected
2. In the middle in blue - what's pending
3. At the bottom a bar with 3 things: time, version and a green/red dot for quality.

When you turn it on you see the Mysterium logo for 5 seconds and then it goes into the dashboard.

If you touch the screen it refreshes instantly. If you press the i in the corner you enter the System Info page where you see the IP, Wi-Fi signal, CPU temperature, etc.

The most useful part - the Web page

You don't have to stay next to it. The ESP also creates a web page that looks IDENTICAL to the screen.

You go from your phone or laptop to http://ip_of_the_esp and you see your earnings in real time, from anywhere in the house. The page updates itself every second.

You also have the /info page with all the node details.

Basically it's a permanent monitor for your node. You put it on your desk next to the router and you always know if your node is running, how much it produces and if it has quality issues.

Very useful if you have multiple nodes or if you don't want to keep a big monitor on just for that.

What do you need?

A CYD module for ~$15 and a Raspberry Pi where your Mysterium Node is already running. That's it.

You configure it once with your Wi-Fi name and the IP of the Pi and that's it. Forget about it.









