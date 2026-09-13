// =============================================================================
// NKSerialization/Reflection/NkReflectSerializer.cpp
// Implementation du pont Reflection <-> Serialization.
// =============================================================================

#include "NKSerialization/Reflection/NkReflectSerializer.h"

#include "NKReflection/NkProperty.h"
#include "NKReflection/NkType.h"
#include "NKReflection/NkReflectVariant.h"
#include "NKReflection/NkEnumDescriptor.h"
#include "NKReflection/NkContainerTrait.h"

namespace nkentseu {

	using reflection::NkClass;
	using reflection::NkContainerDescriptor;
	using reflection::NkEnumDescriptor;
	using reflection::NkEnumRegistry;
	using reflection::NkProperty;
	using reflection::NkPropertyFlags;
	using reflection::NkReflectVariant;
	using reflection::NkType;
	using reflection::NkTypeCategory;

	// -------------------------------------------------------------------------
	// UTILITAIRES INTERNES
	// -------------------------------------------------------------------------
	namespace {

		// Vrai si la categorie est un entier signe ou non signe (incluant bool
		// traite ailleurs). Sert a router vers SetInt64.
		nk_bool IsIntegerCategory(NkTypeCategory c) {
			switch (c) {
				case NkTypeCategory::NK_INT8:
				case NkTypeCategory::NK_INT16:
				case NkTypeCategory::NK_INT32:
				case NkTypeCategory::NK_INT64:
				case NkTypeCategory::NK_UINT8:
				case NkTypeCategory::NK_UINT16:
				case NkTypeCategory::NK_UINT32:
				case NkTypeCategory::NK_UINT64:
					return true;
				default:
					return false;
			}
		}

		// Ecrit une propriete primitive/string/enum dans l'archive.
		// Retourne true si la categorie a ete prise en charge.
		nk_bool WriteScalarProperty(const NkProperty *prop, const void *instance, NkArchive &ar) {
			const NkType &type = prop->GetType();
			const NkTypeCategory cat = type.GetCategory();
			const nk_char *key = prop->GetName();

			NkReflectVariant v = prop->GetValueGeneric(instance);

			switch (cat) {
				case NkTypeCategory::NK_BOOL:
					ar.SetBool(key, v.ToBool());
					return true;

				case NkTypeCategory::NK_FLOAT32:
				case NkTypeCategory::NK_FLOAT64:
					ar.SetFloat64(key, v.ToFloat64());
					return true;

				case NkTypeCategory::NK_STRING:
					ar.SetString(key, v.ToString().View());
					return true;

				// Enums : sauves comme NOM symbolique si un descripteur est
				// disponible (robuste au reordering), sinon comme entier.
				case NkTypeCategory::NK_ENUM: {
					const nk_int64 ev = v.ToInt64();
					const NkEnumDescriptor *ed = NkEnumRegistry::Get().Find(type.GetName());
					const nk_char *sym = ed ? ed->ToName(ev) : nullptr;
					if (sym) {
						ar.SetString(key, NkStringView(sym));
					} else {
						ar.SetInt64(key, ev);
					}
					return true;
				}

				default:
					if (IsIntegerCategory(cat)) {
						ar.SetInt64(key, v.ToInt64());
						return true;
					}
					return false;
			}
		}

		// Aide : ecrit un entier 64 bits vers une propriete entiere/enum en
		// respectant la categorie destination (la coercition finale est faite
		// par SetValueGeneric/WritePrimitiveCoerced cote NkProperty).
		// On passe par un variant de meme categorie que la destination quand
		// possible, sinon un variant int64 que NkProperty saura coercer.
		nk_bool WriteIntToProperty(const NkProperty *prop, void *instance, const NkType &type, nk_int64 value) {
			switch (type.GetCategory()) {
				case NkTypeCategory::NK_INT8:
					return prop->SetValueGeneric(instance,
												 NkReflectVariant::From<nk_int8>(static_cast<nk_int8>(value)));
				case NkTypeCategory::NK_INT16:
					return prop->SetValueGeneric(instance,
												 NkReflectVariant::From<nk_int16>(static_cast<nk_int16>(value)));
				case NkTypeCategory::NK_INT32:
					return prop->SetValueGeneric(instance,
												 NkReflectVariant::From<nk_int32>(static_cast<nk_int32>(value)));
				case NkTypeCategory::NK_INT64:
					return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_int64>(value));
				case NkTypeCategory::NK_UINT8:
					return prop->SetValueGeneric(instance,
												 NkReflectVariant::From<nk_uint8>(static_cast<nk_uint8>(value)));
				case NkTypeCategory::NK_UINT16:
					return prop->SetValueGeneric(instance,
												 NkReflectVariant::From<nk_uint16>(static_cast<nk_uint16>(value)));
				case NkTypeCategory::NK_UINT32:
					return prop->SetValueGeneric(instance,
												 NkReflectVariant::From<nk_uint32>(static_cast<nk_uint32>(value)));
				case NkTypeCategory::NK_UINT64:
					return prop->SetValueGeneric(instance,
												 NkReflectVariant::From<nk_uint64>(static_cast<nk_uint64>(value)));
				default:
					// Enum ou autre : on ecrit en int64, SetValueGeneric coerce
					// vers la taille reelle de la destination.
					return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_int64>(value));
			}
		}

		// Lit une propriete primitive/string/enum depuis l'archive et l'ecrit
		// dans l'instance via SetValueGeneric. Retourne true si la cle existait
		// et la categorie etait geree.
		nk_bool ReadScalarProperty(const NkProperty *prop, void *instance, const NkArchive &ar) {
			const NkType &type = prop->GetType();
			const NkTypeCategory cat = type.GetCategory();
			const nk_char *key = prop->GetName();

			switch (cat) {
				case NkTypeCategory::NK_BOOL: {
					nk_bool b = false;
					if (!ar.GetBool(key, b)) {
						return false;
					}
					return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_bool>(b));
				}

				case NkTypeCategory::NK_FLOAT32: {
					nk_float64 d = 0.0;
					if (!ar.GetFloat64(key, d)) {
						return false;
					}
					return prop->SetValueGeneric(instance,
												 NkReflectVariant::From<nk_float32>(static_cast<nk_float32>(d)));
				}

				case NkTypeCategory::NK_FLOAT64: {
					nk_float64 d = 0.0;
					if (!ar.GetFloat64(key, d)) {
						return false;
					}
					return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_float64>(d));
				}

				case NkTypeCategory::NK_STRING: {
					NkString s;
					if (!ar.GetString(key, s)) {
						return false;
					}
					return prop->SetValueGeneric(instance, NkReflectVariant::From<NkString>(s));
				}

				case NkTypeCategory::NK_ENUM: {
					// Enum : tente d'abord la lecture par NOM symbolique (string),
					// sinon repli sur l'entier. Resoud la valeur sous-jacente puis
					// ecrit en respectant la taille de la destination.
					nk_int64 i = 0;
					NkString sym;
					if (ar.GetString(key, sym)) {
						const NkEnumDescriptor *ed = NkEnumRegistry::Get().Find(type.GetName());
						if (!ed || !ed->ToValue(sym.CStr(), i)) {
							return false;
						}
					} else if (!ar.GetInt64(key, i)) {
						return false;
					}
					return WriteIntToProperty(prop, instance, type, i);
				}

				default: {
					if (IsIntegerCategory(cat)) {
						nk_int64 i = 0;
						if (!ar.GetInt64(key, i)) {
							return false;
						}
						// On construit un variant DE LA BONNE TAILLE/TYPE de la
						// propriete a partir des octets de i. SetValueGeneric
						// recoit donc un variant primitif coerce ; il sait
						// ecrire en respectant la taille destination.
						return WriteIntToProperty(prop, instance, type, i);
					}
					return false;
				}
			}
		}

		// ---------------------------------------------------------------------
		// CONTENEURS (Phase 3) — NkVector<primitif/string> via SetArray/GetArray
		// ---------------------------------------------------------------------

		// Lit la valeur de l'element a elemPtr (de categorie elemCat) et la
		// convertit en NkArchiveValue scalaire. Retourne false si non gere.
		nk_bool ElementToValue(const void *elemPtr, const NkType &elemType, NkArchiveValue &out) {
			const NkTypeCategory cat = elemType.GetCategory();

			if (cat == NkTypeCategory::NK_STRING) {
				const NkString *s = static_cast<const NkString *>(elemPtr);
				out = NkArchiveValue::FromString(s->View());
				return true;
			}

			// Primitifs / enum : copie binaire dans un variant pour coercion.
			NkReflectVariant v = NkReflectVariant::FromRaw(&elemType, elemPtr);
			if (!v.IsValid()) {
				return false;
			}
			switch (cat) {
				case NkTypeCategory::NK_BOOL:
					out = NkArchiveValue::FromBool(v.ToBool());
					return true;
				case NkTypeCategory::NK_FLOAT32:
				case NkTypeCategory::NK_FLOAT64:
					out = NkArchiveValue::FromFloat64(v.ToFloat64());
					return true;
				case NkTypeCategory::NK_ENUM:
					out = NkArchiveValue::FromInt64(v.ToInt64());
					return true;
				default:
					if (IsIntegerCategory(cat)) {
						out = NkArchiveValue::FromInt64(v.ToInt64());
						return true;
					}
					return false;
			}
		}

		// Ecrit une NkArchiveValue scalaire dans l'element a elemPtr (categorie
		// elemCat), avec coercion. Retourne false si non gere.
		nk_bool ValueToElement(const NkArchiveValue &val, void *elemPtr, const NkType &elemType) {
			const NkTypeCategory cat = elemType.GetCategory();

			if (cat == NkTypeCategory::NK_STRING) {
				NkString *s = static_cast<NkString *>(elemPtr);
				*s = val.text; // representation textuelle canonique
				return true;
			}

			// Recupere une valeur numerique depuis la NkArchiveValue.
			nk_int64 iv = 0;
			nk_float64 fv = 0.0;
			nk_bool isFloat = false;
			if (val.IsFloat()) {
				fv = val.raw.f;
				isFloat = true;
			} else if (val.IsInt()) {
				iv = val.raw.i;
			} else if (val.IsUInt()) {
				iv = static_cast<nk_int64>(val.raw.u);
			} else if (val.IsBool()) {
				iv = val.raw.b ? 1 : 0;
			} else {
				return false;
			}

			nk_uint8 *dst = static_cast<nk_uint8 *>(elemPtr);
			auto putBytes = [&](const void *src, nk_usize n) {
				const nk_uint8 *s = static_cast<const nk_uint8 *>(src);
				for (nk_usize k = 0; k < n; ++k) {
					dst[k] = s[k];
				}
			};

			switch (cat) {
				case NkTypeCategory::NK_BOOL: {
					nk_bool b = isFloat ? (fv != 0.0) : (iv != 0);
					putBytes(&b, sizeof(b));
					return true;
				}
				case NkTypeCategory::NK_INT8: {
					nk_int8 v = static_cast<nk_int8>(isFloat ? (nk_int64)fv : iv);
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_INT16: {
					nk_int16 v = static_cast<nk_int16>(isFloat ? (nk_int64)fv : iv);
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_INT32: {
					nk_int32 v = static_cast<nk_int32>(isFloat ? (nk_int64)fv : iv);
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_INT64: {
					nk_int64 v = isFloat ? (nk_int64)fv : iv;
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_UINT8: {
					nk_uint8 v = static_cast<nk_uint8>(isFloat ? (nk_int64)fv : iv);
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_UINT16: {
					nk_uint16 v = static_cast<nk_uint16>(isFloat ? (nk_int64)fv : iv);
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_UINT32: {
					nk_uint32 v = static_cast<nk_uint32>(isFloat ? (nk_int64)fv : iv);
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_UINT64: {
					nk_uint64 v = static_cast<nk_uint64>(isFloat ? (nk_int64)fv : iv);
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_FLOAT32: {
					nk_float32 v = static_cast<nk_float32>(isFloat ? fv : (nk_float64)iv);
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_FLOAT64: {
					nk_float64 v = isFloat ? fv : (nk_float64)iv;
					putBytes(&v, sizeof(v));
					return true;
				}
				case NkTypeCategory::NK_ENUM: {
					// Ecrit selon la taille du type d'enum.
					nk_int64 v = isFloat ? (nk_int64)fv : iv;
					putBytes(&v, elemType.GetSize() <= sizeof(nk_int64) ? elemType.GetSize() : sizeof(nk_int64));
					return true;
				}
				default:
					return false;
			}
		}

		// Declarations anticipees (recursion conteneur<objet> <-> SerializeReflected).
		// SerializeReflected/DeserializeReflected sont membres de NkReflectSerializer.

		// Serialise un conteneur d'OBJETS reflechis (NkVector<SousObjet NK_CLASS>)
		// en tableau d'objets (object-array). Chaque element est serialise
		// recursivement via SerializeReflected sur le NkClass de l'element.
		// Retourne true si pris en charge (conteneur d'objets reflechis).
		nk_bool WriteObjectContainerProperty(const NkProperty *prop, const void *instance, const NkType &elemType,
											 NkArchive &ar) {
			const NkClass *elemCls = elemType.GetClass();
			if (!elemCls) {
				return false; // element NK_CLASS non reflechi (pas de NkClass lie)
			}
			const NkContainerDescriptor *desc = prop->GetContainer();
			const void *container = prop->GetValuePtr(instance);
			const nk_usize count = desc->GetCount(container);

			NkVector<NkArchive> objects;
			for (nk_usize i = 0; i < count; ++i) {
				const void *elemPtr = desc->GetElementPtr(container, i);
				if (!elemPtr) {
					continue;
				}
				NkArchive sub;
				NkReflectSerializer::SerializeReflected(elemCls, elemPtr, sub);
				objects.PushBack(sub);
			}
			ar.SetObjectArray(prop->GetName(), objects);
			return true;
		}

		// Serialise une propriete conteneur (NkVector<primitif/string>) en
		// tableau de scalaires. Pour un conteneur d'objets reflechis, delegue a
		// WriteObjectContainerProperty (object-array). Retourne true si pris en
		// charge.
		nk_bool WriteContainerProperty(const NkProperty *prop, const void *instance, NkArchive &ar) {
			const NkContainerDescriptor *desc = prop->GetContainer();
			if (!desc || !desc->IsValid() || !desc->elementType) {
				return false;
			}
			const NkType &elemType = *desc->elementType;
			// Conteneur d'objets reflechis : object-array recursif.
			if (elemType.GetCategory() == NkTypeCategory::NK_CLASS) {
				return WriteObjectContainerProperty(prop, instance, elemType, ar);
			}

			const void *container = prop->GetValuePtr(instance);
			const nk_usize count = desc->GetCount(container);

			NkVector<NkArchiveValue> arr;
			for (nk_usize i = 0; i < count; ++i) {
				const void *elemPtr = desc->GetElementPtr(container, i);
				NkArchiveValue val;
				if (elemPtr && ElementToValue(elemPtr, elemType, val)) {
					arr.PushBack(val);
				}
			}
			ar.SetArray(prop->GetName(), arr);
			return true;
		}

		// Deserialise un conteneur d'OBJETS reflechis depuis un object-array.
		// Vide le conteneur, puis pour chaque sous-objet de l'archive : ajoute un
		// element par defaut (PushBackDefault) et le remplit recursivement via
		// DeserializeReflected sur le NkClass de l'element.
		nk_bool ReadObjectContainerProperty(const NkProperty *prop, void *instance, const NkType &elemType,
											const NkArchive &ar) {
			const NkClass *elemCls = elemType.GetClass();
			if (!elemCls) {
				return false;
			}
			NkVector<NkArchive> objects;
			if (!ar.GetObjectArray(prop->GetName(), objects)) {
				return false;
			}
			const NkContainerDescriptor *desc = prop->GetContainer();
			void *container = prop->GetValuePtr(instance);
			desc->Clear(container);
			for (nk_usize i = 0; i < objects.Size(); ++i) {
				void *elemPtr = desc->PushBackDefault(container);
				if (elemPtr) {
					NkReflectSerializer::DeserializeReflected(elemCls, elemPtr, objects[i]);
				}
			}
			return true;
		}

		// Deserialise une propriete conteneur depuis un tableau de scalaires (ou
		// object-array pour un conteneur d'objets reflechis).
		nk_bool ReadContainerProperty(const NkProperty *prop, void *instance, const NkArchive &ar) {
			const NkContainerDescriptor *desc = prop->GetContainer();
			if (!desc || !desc->IsValid() || !desc->elementType) {
				return false;
			}
			const NkType &elemType = *desc->elementType;
			if (elemType.GetCategory() == NkTypeCategory::NK_CLASS) {
				return ReadObjectContainerProperty(prop, instance, elemType, ar);
			}

			NkVector<NkArchiveValue> arr;
			if (!ar.GetArray(prop->GetName(), arr)) {
				return false;
			}

			void *container = prop->GetValuePtr(instance);
			desc->Clear(container);
			for (nk_usize i = 0; i < arr.Size(); ++i) {
				void *elemPtr = desc->PushBackDefault(container);
				if (elemPtr) {
					ValueToElement(arr[i], elemPtr, elemType);
				}
			}
			return true;
		}

	} // namespace

	// -------------------------------------------------------------------------
	// SerializeReflected
	// -------------------------------------------------------------------------
	nk_bool NkReflectSerializer::SerializeReflected(const NkClass *cls, const void *instance, NkArchive &ar) {
		if (!cls || !instance) {
			return false;
		}

		// Parcours de la chaine d'heritage : on traite la classe courante PUIS
		// ses bases, de sorte que toutes les proprietes (heritees comprises)
		// soient serialisees. La deduplication par nom est assuree en amont par
		// NkClass::AddProperty.
		// ⚠️ VERITE DU RETOUR. Jusqu'au 2026-08-22, cette fonction rendait `true`
		// INCONDITIONNELLEMENT, en ayant silencieusement omis toute propriete
		// qu'elle ne savait pas ecrire. Mesure qui l'a etabli (banc C5) : un
		// `const char *` du kit (un NOM DE METRIQUE) disparaissait de l'archive,
		// `SerializeObject` rendait `true`, et la relecture donnait une chaine
		// vide sans le moindre signal -- une taille qui designait la metrique
		// « largeur_palette » se resolvait au NOMBRE.
		//
		// Regle du depot : **un repli qui preserve `success` n'est pas un repli,
		// c'est un mensonge.** On ECRIT TOUT CE QU'ON PEUT -- une archive
		// partielle vaut mieux que rien, et l'appelant peut l'inspecter -- mais
		// on rend `false` des qu'une seule propriete a ete perdue.
		nk_bool ok = true;

		for (const NkClass *current = cls; current != nullptr; current = current->GetBaseClass()) {
			const nk_usize count = current->GetPropertyCount();
			for (nk_usize i = 0; i < count; ++i) {
				const NkProperty *prop = current->GetPropertyAt(i);
				if (!prop) {
					continue;
				}

				// Exclusion des proprietes transientes et statiques (les
				// statiques ne sont pas liees a l'instance). Ce ne sont PAS des
				// pertes : leur absence est voulue et documentee.
				if (prop->IsTransient() || prop->IsStatic()) {
					continue;
				}

				const NkType &type = prop->GetType();
				const NkTypeCategory cat = type.GetCategory();

				// Cas conteneur reflechi (NkVector<...>) : tableau de scalaires.
				if (prop->IsContainer()) {
					if (!WriteContainerProperty(prop, instance, ar)) {
						ok = false; // conteneur perdu : ne PAS le taire
					}
					continue;
				}

				// Cas objet imbrique reflechi : NK_CLASS avec NkClass associe.
				if (cat == NkTypeCategory::NK_CLASS) {
					const NkClass *subCls = type.GetClass();
					if (!subCls) {
						// NK_CLASS sans NkClass associe : le type n'est pas
						// reflechi, la propriete est PERDUE. C'est un fait, pas
						// un detail d'implementation.
						ok = false;
						continue;
					}
					const void *subInstance = prop->GetValuePtr(instance);
					NkArchive subAr;
					// On pose le sous-objet MEME s'il est partiel : ce qui a pu
					// etre ecrit est ecrit, et l'echec remonte par `ok`.
					if (!SerializeReflected(subCls, subInstance, subAr)) {
						ok = false;
					}
					ar.SetObject(prop->GetName(), subAr);
					continue;
				}

				// Cas scalaires/string/enum. Une categorie non geree
				// (NK_POINTER, dont `const char *`) tombe ici et rend `false` :
				// c'est la dette des pointeurs, desormais VISIBLE au lieu d'etre
				// silencieuse.
				if (!WriteScalarProperty(prop, instance, ar)) {
					ok = false;
				}
			}
		}

		return ok;
	}

	// -------------------------------------------------------------------------
	// DeserializeReflected
	// -------------------------------------------------------------------------
	nk_bool NkReflectSerializer::DeserializeReflected(const NkClass *cls, void *instance, const NkArchive &ar) {
		if (!cls || !instance) {
			return false;
		}

		// ⚠️ VERITE DU RETOUR, cote lecture -- et la regle N'EST PAS la meme qu'en
		// ecriture. Une cle ABSENTE de l'archive est LEGITIME : champ optionnel,
		// document ecrit par une version anterieure, valeur laissee au defaut.
		// La traiter en erreur casserait la compatibilite ascendante.
		//
		// Ce qui n'est PAS legitime, c'est une cle PRESENTE que le lecteur ne
		// sait pas relire : la donnee est dans le fichier, elle n'arrive pas
		// dans l'objet, et personne n'en est averti.
		nk_bool ok = true;

		for (const NkClass *current = cls; current != nullptr; current = current->GetBaseClass()) {
			const nk_usize count = current->GetPropertyCount();
			for (nk_usize i = 0; i < count; ++i) {
				const NkProperty *prop = current->GetPropertyAt(i);
				if (!prop) {
					continue;
				}

				// Transient/static/read-only : non ecrites. Absence voulue.
				if (prop->IsTransient() || prop->IsStatic() || prop->IsReadOnly()) {
					continue;
				}

				// La cle n'est pas dans l'archive : cas NORMAL, on passe.
				const nk_bool present = ar.Has(prop->GetName());

				const NkType &type = prop->GetType();
				const NkTypeCategory cat = type.GetCategory();

				// Conteneur reflechi (NkVector<...>).
				if (prop->IsContainer()) {
					if (!ReadContainerProperty(prop, instance, ar) && present) {
						ok = false; // presente mais illisible : perte
					}
					continue;
				}

				// Objet imbrique reflechi.
				if (cat == NkTypeCategory::NK_CLASS) {
					const NkClass *subCls = type.GetClass();
					if (!subCls) {
						// Type non reflechi : si la cle est la, on ne sait pas
						// la relire et la donnee reste dans le fichier.
						if (present) {
							ok = false;
						}
						continue;
					}
					NkArchive subAr;
					if (ar.GetObject(prop->GetName(), subAr)) {
						void *subInstance = prop->GetValuePtr(instance);
						if (!DeserializeReflected(subCls, subInstance, subAr)) {
							ok = false;
						}
					} else if (present) {
						// La cle existe mais n'est pas un objet : incoherence.
						ok = false;
					}
					continue;
				}

				// Scalaires/string/enum. Une categorie non geree (NK_POINTER)
				// echoue ici ; on ne la signale que si la cle etait presente.
				if (!ReadScalarProperty(prop, instance, ar) && present) {
					ok = false;
				}
			}
		}

		return ok;
	}

} // namespace nkentseu

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
