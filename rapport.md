# PCOLAB06 - rapport

**Autheur** : Mauro Santos, Gabriel Bader

# Overview

Ce projet consiste à optimiser la multiplication de matrices carrées (C = A·B) en utilisant **plusieurs threads** et un **moniteur de Hoare**.

On découpe la matrice résultat en **blocs** et on envoie ces blocs dans un buffer, puis des workers viennent prendre le job.

Le but pédagogique de ce labo est donc de pratiquer les notions de multi-threading, lecteur-readcteur et les moniteurs de Hoare.


# Choix de conception

## Découpage du projet

La modélisation/implémentation de ce projet ce fait comme ceci :

- Les matrices (matrix.h)

`Matrix<T>` et `SquareMatrix<T>` stockent les données dans un `std::vector<T>`.
On a juste `element()` et `setElement()` pour lire/écrire.

- Le multiplicateur simple (simplematrixmultiplier.h)

Version simple du calcul, sert de référence pour le résultat.

- Le multiplicateur multi-thread (threadedmatrixmultiplier.h)

C'est le coeur du labo :
- `ThreadedMatrixMultiplier<T>` : crée les threads au constructeur.
- `Buffer<T>` : file de jobs + synchronisation via moniteur de Hoare.
- `ComputeParameters<T>` : paquet de paramètres (A,B,C + x,y + blockSize).

- Les tests (main.cpp + multipliertester.h + multiplierthreadedtester.h)

Ce sont les test qui valident notre implémentation.

### Moniteur de Hoare (le buffer)

On à plusieurs threads workers qui tournent en boucle et demandent du travail.
On centralise l'état partagé dans un seul endroit : **la file de jobs**.

On a une classe `Buffer<T> : public PcoHoareMonitor`.
L'idée :

- Le thread qui appel `multiply()` est le **producteur** (il push les jobs).
- Les workers sont des **consommateurs** (ils pop un job et le calcule).

Avec les méthodes clé :

- `sendJob(params)` : push dans la queue + `signal(notEmpty)`.
- `getJob(parameters)` : si vide -> `wait(notEmpty)`, sinon pop.
- `jobFinished()` : décrémente les jobs “en cours” et si plus rien → `signal(jobsComplete)`.
- `waitJobs()` : le thread producteur attend que tout les jobs soient finis.
- `waitFree()` : attend que le buffer soit totalement idle (queue vide + 0 jobs dispatched).
- `stop()` : met le flag et réveille tout le monde.


**Pourquoi Hoare et pas juste un mutex ?**

Parce qu'on a besoin de conditions (par exemple pour savoir si les jobs sont fini) -> Moniteur de Hoare bien.


### Lecteur / rédacteurs et variable critique

#### lecture

Dans `doJob`, on lit en boucle :

```c++
value += params.A->element(i, y) * params.B->element(x, i);
```

Donc A et B sont des ressources **lecture seule** pendant la multiplication.
Plusieurs threads peuvent donc les lire simultanément sans problème.

#### Rédacteur

Chaque worker écrit dans C :

```c++
params.C->setElement(x,y,value);
```

Normalement c'est le cas “writers” : si deux threads écrivent la meme case, problème de concurence.
Notre choix de conception pour éviter ça :

**un job = un bloc de C**, donc deux threads ne doivent jamais écrire sur la même zone.

Concrètement : `multiply()` crée des jobs avec un (x,y) de départ et un `blockSize`, et `doJob` calcule toutes les cases du bloc.
Résultat : pas besoin d'un mutex global sur `C` (on garde plus de parallélisme).

#### Le buffer

Le buffer est une ressource partagée modifiée par :
- le producteur (`push_back`)
- les workers (`pop_front`)
- les compteurs (`nbJobsDispatched`, etc.)

Ici producers ET consumers sont des “rédacteurs” de la ressource → d'où le moniteur pour protèger l'accès.


### Réentrance de `multiply()`

Le labo demande que `multiply()` soit réentrante (test `MultiplierThreadedTester`).
Notre choix pour éviter que les jobs de 2 multiplications se mélangent :

- au début de `multiply(...)` on appel `buffer.waitFree()`

Donc si un autre thread appelle `multiply()` pendant qu'un calcul est en cours, il attend que le buffer soit totalement vide.

## Tests

La stratégie générale : on compare toujours la sortie de notre `ThreadedMatrixMultiplier` avec la ref `SimpleMatrixMultiplier`.
C'est `Matrix::compare()` qui affiche la premiere case qui diffère (sinon “No error in calculus”).

### Tests de base

- `SingleThread`

On lance le multiplicateur threadé avec **1 seul thread**. Ca valide la logique sans “vrai” parallélisme.

- `Simple`

On lance avec **4 threads** sur une matrice 500x500. But : valider la correction + voir un gain de temps.

### Réentrance

- `Reentering` : 2 threads appellent `multiply()` en parallele sur la même instance.
- `ReenteringWith3` : pareil mais à 3.

Si on avait un état partagé mal protégé, on verrait des erreurs de calcul ou un deadlock.

### Cas bord (découpage)

- `OddNumber`

Test avec `matrixSize/nbBlocksPerRow` pas rond (501 / 7). On gère ça en adaptant `blockSize` sur la derniere ligne/colonne + un check de limite dans `doJob`.

### Arrêt en plein calcul

- `StopMidMultiplication`

On démarre une multiplication dans un thread, on attend un moment, puis on détruit le multiplicateur.

### Threads en trop

- `UnusedThreads`

On met plus de threads que de jobs utiles.
Les threads en trop doivent juste dormir sur `wait(notEmpty)`.


# Conclusion

Ce labo nous a permis de bien pratiquer les notions théoriques comme vu à l'introduction. Nous sommes satisfait du résultat et nos test montrent que selon nous notre implémentation est fonctionel.

