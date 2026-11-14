#pragma once

#include "Commun.h"

/** Les plugins VST3 installés sur l'ordinateur : liste, rangement par catégorie, chargement. */
namespace Vst
{
    inline const juce::String sansCategorie()       { return fr ("Sans catégorie"); }

    /** Identifiant d'un plugin dans le navigateur : « vsti:... » (instrument) ou « vstfx:... » (effet). */
    inline juce::String idDe (const juce::PluginDescription& d)
    {
        return (d.isInstrument ? "vsti:" : "vstfx:") + d.createIdentifierString();
    }

    inline bool estIdVst (const juce::String& id)           { return id.startsWith ("vsti:") || id.startsWith ("vstfx:"); }

    /** Le nom du plugin, retrouvé dans son identifiant (de la forme « VST3-Nom-xxxx-xxxx »). */
    inline juce::String nomDepuisId (const juce::String& id)
    {
        auto morceaux = juce::StringArray::fromTokens (id.fromFirstOccurrenceOf (":", false, false), "-", "");

        if (morceaux.size() >= 4)
        {
            morceaux.remove (0);
            morceaux.removeRange (morceaux.size() - 2, 2);
            return morceaux.joinIntoString ("-");
        }

        return "Plugin";
    }

    inline juce::Array<juce::PluginDescription> liste (te::Engine& engine)
    {
        auto types = engine.getPluginManager().knownPluginList.getTypes();
        std::sort (types.begin(), types.end(),
                   [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
                   {
                       return a.name.compareIgnoreCase (b.name) < 0;
                   });
        return types;
    }

    inline std::unique_ptr<juce::PluginDescription> trouver (te::Engine& engine, const juce::String& id)
    {
        const auto identifiant = id.fromFirstOccurrenceOf (":", false, false);
        return engine.getPluginManager().knownPluginList.getTypeForIdentifierString (identifiant);
    }

    /** Crée le plugin dans le morceau (sans encore le poser sur une piste). */
    inline te::Plugin::Ptr creer (te::Edit& edit, const juce::String& id)
    {
        if (auto d = trouver (edit.engine, id))
            return edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, *d);

        return {};
    }

    //==========================================================================
    // Rangement par catégorie (un petit fichier texte : identifiant, tabulation, catégorie)

    inline juce::File fichierCategories()       { return Reglages::dossierDonnees().getChildFile ("categories_vst.txt"); }

    inline juce::StringPairArray& categories()
    {
        static juce::StringPairArray c;
        static bool lu = false;

        if (! lu)
        {
            lu = true;
            juce::StringArray lignes;
            lignes.addLines (fichierCategories().loadFileAsString());

            for (auto& l : lignes)
                if (l.contains ("\t"))
                    c.set (l.upToFirstOccurrenceOf ("\t", false, false), l.fromFirstOccurrenceOf ("\t", false, false));
        }

        return c;
    }

    inline juce::String categorieDe (const juce::String& id)
    {
        const auto c = categories()[id];
        return c.isEmpty() ? sansCategorie() : c;
    }

    inline void definirCategorie (const juce::String& id, const juce::String& categorie)
    {
        auto& c = categories();

        if (categorie.isEmpty() || categorie == sansCategorie())
            c.remove (juce::StringRef (id));
        else
            c.set (id, categorie);

        juce::StringArray lignes;
        for (auto& cle : c.getAllKeys())
            lignes.add (cle + "\t" + c[cle]);

        fichierCategories().replaceWithText (lignes.joinIntoString ("\n"));
    }

    /** Les catégories créées par l'utilisateur (sans « Sans catégorie »), triées. */
    inline juce::StringArray categoriesUtilisees()
    {
        juce::StringArray noms;

        for (auto& v : categories().getAllValues())
            noms.addIfNotAlreadyThere (v);

        noms.sort (true);
        return noms;
    }
}

//==============================================================================
/** La fenêtre d'un plugin VST (son interface graphique). */
class FenetrePlugin : public juce::DocumentWindow
{
public:
    FenetrePlugin (te::Plugin& p)
        : juce::DocumentWindow (p.getName(), juce::Colours::black, juce::DocumentWindow::closeButton, true),
          plugin (p)
    {
        getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
        setResizeLimits (100, 50, 4000, 4000);
        setUsingNativeTitleBar (true);

        editeur = plugin.createEditor();

        if (editeur != nullptr)
        {
            setContentNonOwned (editeur.get(), true);
            setResizable (editeur->allowWindowResizing(), false);
        }

        setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());
        pret = true;
    }

    ~FenetrePlugin() override
    {
        pret = false;
        plugin.edit.flushPluginStateIfNeeded (plugin);
        clearContentComponent();
        editeur.reset();
    }

    bool aUnEditeur() const         { return editeur != nullptr; }

    void closeButtonPressed() override              { plugin.windowState->closeWindowExplicitly(); }
    void userTriedToCloseWindow() override          { plugin.windowState->closeWindowExplicitly(); }
    float getDesktopScaleFactor() const override    { return 1.0f; }

    void moved() override
    {
        if (pret)
            plugin.windowState->lastWindowBounds = getBounds();
    }

private:
    te::Plugin& plugin;
    std::unique_ptr<te::Plugin::EditorComponent> editeur;
    bool pret = false;
};
