#include "stdafx.h"

void node::sort_children()
{
    const auto count = m_children.get_count();
    mmh::Permuation permutation(count);
    pfc::array_staticsize_t<pfc::stringcvt::string_wide_from_utf8_fast> sortdata(count);

    for (size_t n = 0; n < count; n++)
        sortdata[n].convert(m_children[n]->m_value);
    mmh::sort_get_permuation(sortdata, permutation, StrCmpLogicalW, false, false, true);

    mmh::destructive_reorder(m_children, permutation);
    concurrency::parallel_for(size_t{0}, count, [this](size_t n) {
        m_children[n]->sort_children();
    });
}

void node::sort_entries()//for contextmenu
{
    if (!m_sorted) {
        pfc::string8 tf_string;
        if (m_bydir)
            tf_string = "%path_sort%";
        else {
            tf_string = m_window->get_hierarchy();
            tf_string += "|%path_sort%";
        }
        service_ptr_t<titleformat_object> script;
        if (static_api_ptr_t<titleformat_compiler>()->compile(script, tf_string))
            fbh::sort_metadb_handle_list_by_format(m_tracks, script, nullptr);
        m_sorted = true;
    }
}

void node::create_new_playlist()
{
    static_api_ptr_t<playlist_manager> api;
    string8 name = m_value.get_ptr();
    if (name.is_empty())
        name = "All music";
    const size_t index = api->create_playlist(name, pfc_infinite, pfc_infinite);
    if (index != pfc_infinite) {
        api->set_active_playlist(index);
        send_to_playlist(true);
    }
}

void node::send_to_playlist(bool replace)
{
    static_api_ptr_t<playlist_manager> api;
    const bool select = !!cfg_add_items_select;
    api->activeplaylist_undo_backup();
    if (replace)
        api->activeplaylist_clear();
    else if (select)
        api->activeplaylist_clear_selection();
    if (cfg_add_items_use_core_sort)
        api->activeplaylist_add_items_filter(m_tracks, select);
    else {
        sort_entries();
        api->activeplaylist_add_items(m_tracks, bit_array_val(select));
    }

    if (select && !replace) {
        const size_t num = api->activeplaylist_get_item_count();
        if (num > 0) {
            api->activeplaylist_set_focus_item(num - 1);
        }
    }
}

node::node(const char* p_value, unsigned p_value_len, album_list_window* window, uint16_t level)
    : m_level(level), m_window(window)
{
    if (p_value && p_value_len > 0) {
        m_value.set_string(p_value, p_value_len);
    }
    m_sorted = false;
}

void node::remove_entries(pfc::bit_array& mask)
{
    m_tracks.remove_mask(mask);
}

void node::set_data(const list_base_const_t<metadb_handle_ptr>& p_data, bool b_keep_existing)
{
    if (!b_keep_existing)
        m_tracks.remove_all();
    m_tracks.add_items(p_data);
    m_sorted = false;
}

node_ptr node::find_or_add_child(const char* p_value, unsigned p_value_len, bool b_find, bool& b_new)
{
    if (!b_find)
        return add_child_v2(p_value, p_value_len);
    if (p_value_len == 0 || *p_value == 0) {
        p_value = "?";
        p_value_len = 1;
    }
    t_size index = 0;
    b_new = true;
    if (m_children.bsearch_t(g_compare_name, uTS(p_value, p_value_len), index)) {
        b_new = false;
    }
    else
        m_children.insert_item(new node(p_value, p_value_len, m_window, m_level + 1), index);
    return m_children[index];
}

node_ptr node::add_child_v2(const char* p_value, unsigned p_value_len)
{
    if (p_value_len == 0 || *p_value == 0) {
        p_value = "?";
        p_value_len = 1;
    }
    node_ptr temp = new node(p_value, p_value_len, m_window, m_level + 1);
    m_children.add_item(temp);
    return temp;
}

void node::mark_all_labels_dirty()
{
    m_label_dirty = true;
    t_size i = m_children.get_count();
    for (; i; i--) {
        m_children[i - 1]->mark_all_labels_dirty();
    }
}

void node::purge_empty_children(HWND wnd)
{
    size_t index_first_removed = pfc_infinite;
    for (size_t i = m_children.get_count(); i; i--) {
        auto& child = m_children[i - 1];
        if (!child->get_entries().get_count()) {
            if (m_window && m_window->m_selection == child)
                m_window->m_selection = nullptr;
            if (child->m_ti)
            TreeView_DeleteItem(wnd, child->m_ti);
            m_children.remove_by_idx(i - 1);
            index_first_removed = i - 1;
        }
    }

    if (index_first_removed != pfc_infinite && cfg_show_subitem_counts)
        m_label_dirty = true;

    if (index_first_removed != pfc_infinite && cfg_show_item_indices) {
        const t_size count = m_children.get_count();
        for (size_t i = index_first_removed; i < count; i++) {
            m_children[i]->m_label_dirty = true;
        }
    }
}
