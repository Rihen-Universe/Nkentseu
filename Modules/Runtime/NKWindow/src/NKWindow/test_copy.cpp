#include "NkEvent.h"
#include "NkTypedEvents.h"

using namespace nkentseu;

int main() {
    // Test 1: Copier un NkWindowShownEvent directement
    NkWindowShownEvent event1;
    event1.timestamp = 100;
    
    // Ceci devrait compiler si le copy constructor existe
    NkWindowShownEvent event2 = event1;  // <- ligne qui échoue
    
    // Test 2: Assignment
    NkWindowShownEvent event3;
    event3 = event2;  // <- peut aussi échouer
    
    return 0;
}
