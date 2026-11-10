#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"

// ELMonkey 0.11.12.2 (SAB): Menuebefehl „Bodenfliese aus Auswahl“.
// Setzt fuer jede ausgewaehlte geschlossene Polylinie, Schraffur, Decke oder Zone
// ein Objekt „SAB Bodenfliese liegend“ mit genau diesem Umriss.
// Startpunkt = erste Ecke, Raster entlang der ersten Kante; die uebrigen
// Einstellungen kommen aus dem Standard des Objektwerkzeugs, wenn dort die
// Bodenfliese eingestellt ist, sonst aus den Voreinstellungen des Objekts.
GSErrCode ELM_BodenflieseAusAuswahl ();
