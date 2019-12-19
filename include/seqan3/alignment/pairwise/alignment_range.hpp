// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2019, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2019, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/seqan3/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

/*!\file
 * \brief Provides seqan3::detail::alignment_range.
 * \author Rene Rahn <rene.rahn AT fu-berlin.de>
 */

#pragma once

#include <seqan3/std/concepts>
#include <seqan3/std/ranges>

namespace seqan3
{

/*!\brief An input range over the alignment results generated by the underlying alignment executor.
 * \ingroup pairwise_alignment
 * \implements std::ranges::input_range
 *
 * \tparam alignment_executor_type The type of the underlying alignment executor; must be of type
 *                                 seqan3::detail::alignment_executor_two_way.
 *
 * \details
 *
 * Provides a lazy input-range interface over the alignment results generated by the underlying alignment
 * executor. The alignments are computed in a lazy fashion, such that when incrementing the next alignment
 * result is fetched from the executor. The alignment result will be cached such that dereferencing the
 * associated iterator is constant.
 *
 * \if DEV
 * Note that the required type is not enforced in order to test this class without adding the entire machinery for
 * the seqan3::detail::alignment_executor_two_way.
 * \endif
 */
template <typename alignment_executor_type>
class alignment_range
{
    static_assert(!std::is_const_v<alignment_executor_type>,
                  "Cannot create an alignment stream over a const buffer.");

    class alignment_range_iterator;

    //!\brief Befriend the iterator type.
    friend class alignment_range_iterator;

public:
    //!\brief The offset type.
    using difference_type = typename alignment_executor_type::difference_type;
    //!\brief The alignment result type.
    using value_type      = typename alignment_executor_type::value_type;
    //!\brief The reference type.
    using reference       = typename alignment_executor_type::reference;
    //!\brief The iterator type.
    using iterator        = alignment_range_iterator;
    //!\brief This range is never const-iterable. The const_iterator is always void.
    using const_iterator  = void;
    //!\brief The sentinel type.
    using sentinel        = std::ranges::default_sentinel_t;

    /*!\name Constructors, destructor and assignment
     * \{
     */
    alignment_range() = default; //!< Defaulted.
    alignment_range(alignment_range const &) = delete; //!< This is a move-only type.
    alignment_range(alignment_range &&) = default; //!< Defaulted.
    alignment_range & operator=(alignment_range const &) = delete; //!< This is a move-only type.
    alignment_range & operator=(alignment_range &&) = default; //!< Defaulted.
    ~alignment_range() = default; //!< Defaulted.

    //!\brief Explicit deletion to forbid copy construction of the underlying executor.
    explicit alignment_range(alignment_executor_type const & _alignment_executor) = delete;

    /*!\brief Constructs a new alignment range by taking ownership over the passed alignment buffer.
     * \tparam _alignment_executor_type The buffer type. Must be the same type as `alignment_executor_type` when
     *                                  references and cv-qualifiers are removed.
     * \param[in] _alignment_executor   The buffer to take ownership from.
     *
     * \details
     *
     * Constructs a new alignment range by taking ownership over the passed alignment buffer.
     */
    explicit alignment_range(alignment_executor_type && _alignment_executor) :
        alignment_executor{new alignment_executor_type{std::move(_alignment_executor)}}
    {}
    //!\}

    /*!\name Iterators
     * \{
     */

    /*!\brief Returns an iterator to the first element of the alignment range.
     * \return An iterator to the first element.
     *
     * \details
     *
     * Invocation of this function will trigger the computation of the first alignment.
     */
    constexpr iterator begin()
    {
        return iterator{*this};
    }

    const_iterator begin() const = delete;
    const_iterator cbegin() const = delete;

    /*!\brief Returns a sentinel signaling the end of the alignment range.
     * \return a sentinel.
     *
     * \details
     *
     * The alignment range is an input range and the end is reached when the internal buffer over the alignment
     * results has signaled end-of-stream.
     */
    constexpr sentinel end() noexcept
    {
        return {};
    }

    constexpr sentinel end() const = delete;
    constexpr sentinel cend() const = delete;
    //!\}

protected:
    /*!\brief Receives the next alignment result from the executor buffer.
     *
     * \returns `true` if a new alignment result could be fetched, otherwise `false`.
     *
     * \details
     *
     * Fetches the next element from the underlying alignment executor if available.
     */
    bool next()
    {
        if (!alignment_executor)
            throw std::runtime_error{"No alignment execution buffer available."};

        if (auto opt = alignment_executor->bump(); opt.has_value())
        {
            cache = std::move(*opt);
            return true;
        }

        return false;
    }

private:
    //!\brief The underlying executor buffer.
    std::unique_ptr<alignment_executor_type> alignment_executor{};
    //!\brief Stores last read element.
    value_type cache{};
};

/*!\name Type deduction guide
 * \relates seqan3::alignment_range
 * \{
 */

//!\brief Deduces from the passed alignment_executor_type
template <typename alignment_executor_type>
alignment_range(alignment_executor_type &&) -> alignment_range<std::remove_reference_t<alignment_executor_type>>;
//!\}

/*!\brief The iterator of seqan3::detail::alignment_range.
 * \implements std::input_iterator
 */
template <typename alignment_executor_type>
class alignment_range<alignment_executor_type>::alignment_range_iterator
{
public:
    //!\brief Type for distances between iterators.
    using difference_type = typename alignment_range::difference_type;
    //!\brief Value type of container elements.
    using value_type = typename alignment_range::value_type;
    //!\brief Use reference type defined by container.
    using reference = typename alignment_range::reference;
    //!\brief Pointer type is pointer of container element type.
    using pointer = std::add_pointer_t<value_type>;
    //!\brief Sets iterator category as input iterator.
    using iterator_category = std::input_iterator_tag;

    /*!\name Constructors, destructor and assignment
     * \{
     */
    constexpr alignment_range_iterator() noexcept = default; //!< Defaulted.
    constexpr alignment_range_iterator(alignment_range_iterator const &) noexcept = default; //!< Defaulted.
    constexpr alignment_range_iterator(alignment_range_iterator &&) noexcept = default; //!< Defaulted.
    constexpr alignment_range_iterator & operator=(alignment_range_iterator const &) noexcept = default; //!< Defaulted.
    constexpr alignment_range_iterator & operator=(alignment_range_iterator &&) noexcept = default; //!< Defaulted.
    ~alignment_range_iterator() = default; //!< Defaulted.

    //!\brief Construct from alignment stream.
    constexpr alignment_range_iterator(alignment_range & range) : range_ptr(& range)
    {
        ++(*this); // Fetch the next element.
    }
    //!\}

    /*!\name Access operators
     * \{
     */

    /*!\brief Access the pointed-to element.
     * \returns A reference to the current element.
     */
    //!\brief Returns the current alignment result.
    reference operator*() const noexcept
    {
        return range_ptr->cache;
    }

    //!\brief Returns a pointer to the current alignment result.
    pointer operator->() const noexcept
    {
        return &range_ptr->cache;
    }
    //!\}

    /*!\name Increment operators
     * \{
     */
    //!\brief Increments the iterator by one.
    alignment_range_iterator & operator++(/*pre*/)
    {
        assert(range_ptr != nullptr);

        at_end = !range_ptr->next();
        return *this;
    }

    //!\brief Returns an iterator incremented by one.
    void operator++(int /*post*/)
    {
        ++(*this);
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    //!\brief Checks whether lhs is equal to the sentinel.
    friend constexpr bool operator==(alignment_range_iterator const & lhs,
                                     std::ranges::default_sentinel_t const &) noexcept
    {
        return lhs.at_end;
    }

    //!\brief Checks whether `lhs` is equal to `rhs`.
    friend constexpr bool operator==(std::ranges::default_sentinel_t const & lhs,
                                     alignment_range_iterator const & rhs) noexcept
    {
        return rhs == lhs;
    }

    //!\brief Checks whether `*this` is not equal to the sentinel.
    friend constexpr bool operator!=(alignment_range_iterator const & lhs,
                                     std::ranges::default_sentinel_t const & rhs) noexcept
    {
        return !(lhs == rhs);
    }

    //!\brief Checks whether `lhs` is not equal to `rhs`.
    friend constexpr bool operator!=(std::ranges::default_sentinel_t const & lhs,
                                     alignment_range_iterator const & rhs) noexcept
    {
        return rhs != lhs;
    }
    //!\}

private:
    //!\brief Pointer to the underlying range.
    alignment_range * range_ptr{};
    //!\brief Indicates the end of the underlying resource.
    bool at_end{true};
};

} // namespace seqan3
