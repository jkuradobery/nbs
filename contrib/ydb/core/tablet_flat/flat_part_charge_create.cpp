#include "flat_part_charge_create.h"
#include "flat_part_charge.h"
#include "flat_part_charge_btree_index.h"

namespace NKikimr::NTable {

THolder<ICharge> CreateCharge(IPages *env, const TPart &part, TTagsRef tags, bool includeHistory) {
    if (part.IndexPages.BTreeGroups) {
        return MakeHolder<TChargeBTreeIndex>(env, part, tags, includeHistory);
    } else {
        return MakeHolder<TCharge>(env, part, tags, includeHistory);
    }
}

}
