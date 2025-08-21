#include "../common/GameManagerRegistration.h"

extern "C" {
    /**
     * Initialization function called when the GameManager library is dynamically loaded.
     * This triggers the GameManager registration singleton.
     */
    void initialize_gamemanager_212934582_323964676() {
        // The registration happens automatically when the static variables
        // in the GameManager registration are initialized.
        // This function just needs to exist to ensure the library is properly loaded.
    }
}
